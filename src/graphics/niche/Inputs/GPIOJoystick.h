#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

/*

Re-usable NicheGraphics input source

4 x directional inputs (Up, Down, Left, Right), and center press
Interrupt driven

*/

#pragma once

#include "configuration.h"

#include "assert.h"
#include "functional"

#ifdef ARCH_ESP32
#include "esp_sleep.h" // For light-sleep handling
#endif

#include "Observer.h"

namespace NicheGraphics::Inputs
{

class GPIOJoystick : private concurrency::OSThread
{
  public:
    enum Direction : uint8_t { UP, RIGHT, DOWN, LEFT, CENTER };
    enum WiringType : uint8_t { UNSET, ACTIVE_LOW, ACTIVE_HIGH, ACTIVE_LOW_PULLUP, ACTIVE_HIGH_PULLDOWN };
    typedef std::function<void()> Callback;

    static GPIOJoystick *getInstance();                     // Create or get the singleton instance
    void start();                                           // Start handling button input
    void stop();                                            // Stop handling button input (disconnect ISRs for sleep)
    void setWiringType(WiringType wiringType);              // Active logic and pull-up / pull-down
    void setDebounce(uint32_t milliseconds);                // Discard events shorter than this
    void setPin(Direction direction, uint8_t pin);          // Set GPIO pin for each direction
    void setHandler(Direction direction, Callback onPress); // Set what code to run when input occurs

    void handlePinChange(Direction direction); // Schedule an input event to be handled shortly, outside the ISRs

    // Disconnect and reconnect interrupts for light sleep
#ifdef ARCH_ESP32
    int beforeLightSleep(void *unused);
    int afterLightSleep(esp_sleep_wakeup_cause_t cause);
#endif

  private:
    GPIOJoystick(); // Constructor made private: force use of GPIOJoystick::getInstance()
    static void noop() {};

    // Need to hard-code an ISR for each GPIO of the joystick
    static void isrUP() { getInstance()->handlePinChange(UP); }
    static void isrRIGHT() { getInstance()->handlePinChange(RIGHT); }
    static void isrDOWN() { getInstance()->handlePinChange(DOWN); }
    static void isrLEFT() { getInstance()->handlePinChange(LEFT); }
    static void isrCENTER() { getInstance()->handlePinChange(CENTER); }

    int32_t runOnce() override; // Timer method. Executes callbacks outside of the ISRs

    WiringType wiring = WiringType::UNSET;
    uint32_t debounceMs = 50;
    uint8_t pins[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Size matches GPIOJoystick::Direction enum
    Callback callbacks[5] = {noop, noop, noop, noop, noop};

    Direction pendingCallback; // Callback to be executed. Set from an ISR, actioned by runOnce()

#ifdef ARCH_ESP32
    // Get notified when lightsleep begins and ends
    CallbackObserver<GPIOJoystick, void *> lsObserver =
        CallbackObserver<GPIOJoystick, void *>(this, &GPIOJoystick::beforeLightSleep);
    CallbackObserver<GPIOJoystick, esp_sleep_wakeup_cause_t> lsEndObserver =
        CallbackObserver<GPIOJoystick, esp_sleep_wakeup_cause_t>(this, &GPIOJoystick::afterLightSleep);
#endif
};

} // namespace NicheGraphics::Inputs

#endif