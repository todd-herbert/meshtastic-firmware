#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "./GPIOJoystick.h"

#include "main.h"
#include "sleep.h"

using namespace NicheGraphics::Inputs;

GPIOJoystick::GPIOJoystick() : concurrency::OSThread("GPIOJoystick")
{
    // Don't start our timer
    // We only enable it from the ISRs, to run callbacks at next loop()
    OSThread::disable();

#ifdef ARCH_ESP32
    // Register callbacks for before and after lightsleep
    lsObserver.observe(&notifyLightSleep);
    lsEndObserver.observe(&notifyLightSleepEnd);
#endif
}

// Get access to (or create) the singleton instance of this class
// Accessible inside the ISRs, even though we maybe shouldn't
GPIOJoystick *GPIOJoystick::getInstance()
{
    // Instantiate the class the first time this method is called
    static GPIOJoystick *const singletonInstance = new GPIOJoystick;

    return singletonInstance;
}

// Set whether input is LOW or HIGH when pressed
// Set whether input uses the internal pulldown / pullup resistors
// Value applies to all of the joystick's inputs
// Need to store this info, as we detach and reattach inputs during light sleep (ESP32)
void GPIOJoystick::setWiringType(WiringType wiringType)
{
    wiring = wiringType; // Store as private member
}

void GPIOJoystick::setDebounce(uint32_t milliseconds)
{
    debounceMs = milliseconds; // Store as a private member
}

void GPIOJoystick::setPin(Direction direction, uint8_t pin)
{
    pins[direction] = pin; // Store as private member
}

// Specify which code to execute when joystick input happens
// This callback code should be a lambda function in setupNicheGraphics()
// The callback will not be run directly inside the ISR. It will be run "ASAP", at next loop()
void GPIOJoystick::setHandler(Direction direction, Callback onPress)
{
    callbacks[direction] = onPress; // Store as private member
}

// Begin receiving button input
// Attaches interrupts
// Do this after after configuring input, in setupNicheGraphics()
// (Also used internally after ESP32 light sleep)
void GPIOJoystick::start()
{
    assert(wiring != WiringType::UNSET);

    if (pins[Direction::UP] != 0xFF)
        attachInterrupt(pins[Direction::UP], GPIOJoystick::isrUP, CHANGE);

    if (pins[Direction::RIGHT] != 0xFF)
        attachInterrupt(pins[Direction::RIGHT], GPIOJoystick::isrRIGHT, CHANGE);

    if (pins[Direction::DOWN] != 0xFF)
        attachInterrupt(pins[Direction::DOWN], GPIOJoystick::isrDOWN, CHANGE);

    if (pins[Direction::LEFT] != 0xFF)
        attachInterrupt(pins[Direction::LEFT], GPIOJoystick::isrLEFT, CHANGE);

    if (pins[Direction::CENTER] != 0xFF)
        attachInterrupt(pins[Direction::CENTER], GPIOJoystick::isrCENTER, CHANGE);
}

// Stop receiving input
// Handles ESP32 light sleep
void GPIOJoystick::stop()
{
    if (pins[Direction::UP] != 0xFF)
        detachInterrupt(pins[Direction::UP]);

    if (pins[Direction::LEFT] != 0xFF)
        detachInterrupt(pins[Direction::LEFT]);

    if (pins[Direction::DOWN] != 0xFF)
        detachInterrupt(pins[Direction::DOWN]);

    if (pins[Direction::RIGHT] != 0xFF)
        detachInterrupt(pins[Direction::RIGHT]);

    if (pins[Direction::CENTER] != 0xFF)
        detachInterrupt(pins[Direction::CENTER]);
}

// Called by the pins' interrupt methods
// Pins use CHANGE interrupts. This method reads pin to detect whether CHANGE was RISING or FALLING.
// This method is also responsible for debouncing
void GPIOJoystick::handlePinChange(Direction direction)
{
    // Time (in milliseconds) when *any* button was pressed
    // For debouncing only
    static uint32_t pressedAtMs = 0;

    // Decide whether pin was just pressed or just released
    bool pressed;
    if (wiring == WiringType::ACTIVE_HIGH || wiring == WiringType::ACTIVE_HIGH_PULLDOWN)
        pressed = (digitalRead(pins[direction]) == HIGH);
    else
        pressed = (digitalRead(pins[direction]) == LOW);

    // Handle press
    if (pressed) {
        pressedAtMs = millis();
    }

    // Handle release
    else {
        // Debounce
        if (millis() - pressedAtMs > debounceMs) {
            // Have our thread execute the callback ASAP, at next loop()
            // Avoids running the callback inside the ISR
            pendingCallback = direction;
            OSThread::setIntervalFromNow(0);
            OSThread::enabled = true;
            runASAP = true;
        }
    }
}

// Timer method
// When joystick input occurs, the ISR schedules this method, so it can execute our callback at next loop()
// Allows us to avoid running callbacks inside an ISR
int32_t GPIOJoystick::runOnce()
{
    callbacks[pendingCallback](); // Execute callback which was scheduled by an ISRs
    return OSThread::disable();   // Handled. Stop thread
}

#ifdef ARCH_ESP32

// Detach our class' interrupts before lightsleep
// Allows sleep.cpp to configure its own interrupts, which wake the device on user-button press
int TwoButton::beforeLightSleep(void *unused)
{
    stop();
    return 0; // Indicates success
}

// Reconfigure our interrupts
// Our class' interrupts were disconnected during sleep, to allow the user button to wake the device from sleep
int TwoButton::afterLightSleep(esp_sleep_wakeup_cause_t cause)
{
    start();
    return 0; // Indicates success
}

#endif

#endif