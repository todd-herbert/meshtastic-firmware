#pragma once
#include "PowerStatus.h"
#include "concurrency/OSThread.h"
#include "configuration.h"
#ifdef ARCH_ESP32
#include <esp_adc_cal.h>
#include <soc/adc_channel.h>
#endif

#ifdef BAT_MEASURE_ADC_UNIT
extern RTC_NOINIT_ATTR uint64_t RTC_reg_b;
#include "soc/sens_reg.h" // needed for adc pin reset
#endif

#if HAS_TELEMETRY && !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && !defined(ARCH_PORTDUINO)
#include "modules/Telemetry/Sensor/INA219Sensor.h"
#include "modules/Telemetry/Sensor/INA260Sensor.h"
#include "modules/Telemetry/Sensor/INA3221Sensor.h"
extern INA260Sensor ina260Sensor;
extern INA219Sensor ina219Sensor;
extern INA3221Sensor ina3221Sensor;
#endif

class Power : private concurrency::OSThread
{

  public:
    Observable<const meshtastic::PowerStatus *> newStatus;

    Power();

    void shutdown();
    void readPowerStatus();
    virtual bool setup();
    virtual int32_t runOnce() override;
    void setStatusHandler(meshtastic::PowerStatus *handler) { statusHandler = handler; }

  protected:
    meshtastic::PowerStatus *statusHandler;

    /// Setup a xpowers chip axp192/axp2101, return true if found
    bool axpChipInit();
    /// Setup a simple ADC input based battery sensor
    bool analogInit();

  private:
    // open circuit voltage lookup table
    uint8_t low_voltage_counter;
#ifdef DEBUG_HEAP
    uint32_t lastheap;
#endif
};

extern Power *power;