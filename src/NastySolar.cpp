#include "NastySolar.h"
#include "bluefruit.h"
#include "target_specific.h"

// Quick and dirty hack to intercept boot and check voltage
// If too low, refuse to boot; sleep and check again later

void nastySolarBootCheck()
{

    // Set-up to read battery via ADC
    pinMode(BATTERY_PIN, INPUT);
    analogReference(VBAT_AR_INTERNAL);
    analogReadResolution(BATTERY_SENSE_RESOLUTION_BITS);

    // Take multiple samples, average, then convert to mV
    constexpr uint8_t samples = 15;
    uint32_t raw = 0;
    for (uint32_t i = 0; i < samples; i++)
        raw += analogRead(BATTERY_PIN);
    raw = raw / samples;
    uint32_t mv = ADC_MULTIPLIER * ((1000 * AREF_VOLTAGE) / pow(2, BATTERY_SENSE_RESOLUTION_BITS)) * raw;

    // If below the cutoff voltage
    if (mv < NASTYSOLAR_CUTOFF_MV) {
        // Blink the blue LED
        pinMode(PIN_LED2, OUTPUT);
        digitalWrite(PIN_LED2, HIGH);

        Bluefruit.autoConnLed(false);
        Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
        Bluefruit.begin();

        // Shutdown bluetooth for minimum power draw
        Bluefruit.Advertising.stop();
        Bluefruit.setTxPower(-40); // Minimum power

        // Init the radio, to place into a low power state
        SX1262 radio = new Module(SX126X_CS, SX126X_DIO1, SX126X_RESET, SX126X_BUSY);
        radio.begin();
        radio.sleep();
        SPI.end();
        Wire.end();   // Probably redundant
        Serial.end(); // Probably redundant

        // Not sure if we need to set *all* these pins..
        pinMode(PIN_3V3_EN, OUTPUT);
        pinMode(PIN_LED1, OUTPUT);
        pinMode(PIN_LED2, OUTPUT);
        digitalWrite(PIN_LED1, LOW);
        digitalWrite(PIN_LED2, LOW);
        digitalWrite(PIN_3V3_EN, LOW);

        // // Enter "low power mode" (not using soft device)
        // NRF_POWER->TASKS_LOWPWR = 1;

        sd_power_mode_set(NRF_POWER_MODE_LOWPWR);

        // Wait, then reboot
        delay(NASTYSOLAR_CHECK_MINUTES * 60 * 1000UL);
        NVIC_SystemReset();

        // We shouldn't be able to reach here - shutting down / reset
        while (true)
            yield();
    }
}