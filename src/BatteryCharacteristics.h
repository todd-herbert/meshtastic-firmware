/*
 * BatteryCharacteristics.h
 *
 * Historically, the battery type was set at compile-time, with a macro
 * This file adds wrappers which allow the battery characteristics to be set at run-time, by the user
 *
 * OCV() replaces array OCV[]
 * numCells replaces macro NUM_CELLS
 *
 *
 * Alternatively, cell type, count, and a custom OCV LUT can be defined at compile-time
 */

// Ensure that this header is imported once only
#ifdef __BATTERY_CHARACTERISTICS_H__
#error This file should only be imported once, by Power.cpp
#endif
#define __BATTERY_CHARACTERISTICS_H__

#include "NodeDB.h"
#include "configuration.h"

#ifndef NUM_OCV_POINTS
#define NUM_OCV_POINTS 11
#endif

// #define CELL_TYPE_LIFEPO4

// Define the open cell voltage look-up-tables for different battery types
#define OCV_LUT_LIION 4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3420, 3300, 3100
#define OCV_LUT_LIFEPO4 3400, 3350, 3320, 3290, 3270, 3260, 3250, 3230, 3200, 3120, 3000
#define OCV_LUT_LTO 2700, 2560, 2540, 2520, 2500, 2460, 2420, 2400, 2380, 2320, 1500
#define OCV_LUT_NIMH 1400, 1300, 1280, 1270, 1260, 1250, 1240, 1230, 1210, 1150, 1000
#define OCV_LUT_LEADACID 2120, 2090, 2070, 2050, 2030, 2010, 1990, 1980, 1970, 1960, 1950
#define OCV_LUT_ALKALINE 1580, 1400, 1350, 1300, 1280, 1250, 1230, 1190, 1150, 1100, 1000

// Select a battery type, if pre-defined for this variant
#if defined(CELL_TYPE_LIION)
#define OCV_LUT OCV_LUT_LIION
#elif defined(CELL_TYPE_LIFEPO4)
#define OCV_LUT OCV_LUT_LIFEPO4
#elif defined(CELL_TYPE_LEADACID)
#define OCV_LUT OCV_LUT_LEADACID
#elif defined(CELL_TYPE_ALKALINE)
#define OCV_LUT OCV_LUT_ALKALINE
#elif defined(CELL_TYPE_NIMH)
#define OCV_LUT OCV_LUT_NIMH
#elif defined(CELL_TYPE_LTO)
#define OCV_LUT OCV_LUT_LTO
#endif

// --- If battery characteristics set at compile-time ---
#ifdef OCV_LUT

// Check if user has specified number of cells
#ifndef NUM_CELLS
#define NUM_CELLS 1
#endif

// Get open cell voltage
static uint16_t OCV(uint8_t n)
{
    static uint16_t LUT[NUM_OCV_POINTS] = {OCV_LUT};
    return LUT[n];
}

// Get cell count
static uint8_t numCells()
{
    return NUM_CELLS;
}

#else  // --- If battery characteristics set at run-time ---

static uint16_t OCV(uint8_t n)
{
    // Make all LUTs available at run-time
    static const uint16_t LiIon[] = {OCV_LUT_LIION};
    static const uint16_t LiFePO4[] = {OCV_LUT_LIFEPO4};
    static const uint16_t LTO[] = {OCV_LUT_LTO};
    static const uint16_t NiMH[] = {OCV_LUT_NIMH};
    static const uint16_t Alkaline[] = {OCV_LUT_ALKALINE};
    static const uint16_t LeadAcid[] = {OCV_LUT_LEADACID};

    // Will point to OCV values for selected type
    static const uint16_t *LUT = nullptr;

    // If chemistry not yet assigned (first call?)
    if (LUT == nullptr) {

        // If nodeDB not yet init.
        if (!nodeDB) {
            LOG_WARN("Attempted to read battery config before nodeDB available\n");
            return 0; // 0V for now..
        }

        // nodeDB is ready. Load the config
        LOG_INFO("Battery chemistry set at run-time: ");
        switch (config.power.battery_chemistry) {
        case meshtastic_Config_PowerConfig_BatteryChemistry_LIFEPO4:
            LUT = LiFePO4;
            LOG_INFO("LiFePO4\n");
            break;
        case meshtastic_Config_PowerConfig_BatteryChemistry_LTO:
            LUT = LTO;
            LOG_INFO("LiFePO4\n");
            break;
        case meshtastic_Config_PowerConfig_BatteryChemistry_NIMH:
            LUT = NiMH;
            LOG_INFO("NiMH\n");
            break;
        case meshtastic_Config_PowerConfig_BatteryChemistry_ALKALINE:
            LUT = Alkaline;
            LOG_INFO("Alkaline\n");
            break;
        case meshtastic_Config_PowerConfig_BatteryChemistry_LEAD_ACID:
            LUT = LeadAcid;
            LOG_INFO("Lead Acid\n");
            break;
        case meshtastic_Config_PowerConfig_BatteryChemistry_LI_ION:
        case meshtastic_Config_PowerConfig_BatteryChemistry_UNSPECIFIED:
        default:
            LUT = LiIon;
            LOG_INFO("Li-Ion\n");
            break;
        }
    }

    // If already loaded, we jump straight to here
    return LUT[n];
}

// Get cell count (from config.proto)
static uint8_t numCells()
{
    // Cache the value
    // Mostly just so we can log at startup..
    static uint8_t num = 0;
    if (num == 0) {
        if (!nodeDB) {
            LOG_WARN("Attempted to read battery config before nodeDB available\n");
            return 1;
        }
        // nodeDB available - read value
        num = max((uint8_t)1, config.power.battery_cell_count);
        LOG_INFO("Battery cell count set at run-time: %i\n", (int)num); // (cast to int, NRF52 complains about %hu)
    }

    return num;
}
#endif // End of compile-time vs run-time

// "For heltecs with no battery connected, the measured voltage is 2204
// so need to be higher than that, in this case is 2500mV (3000-500)"
// (Note: this is no longer true for all heltec devices. - Todd)
static float noBatVolt()
{
    return (OCV(NUM_OCV_POINTS - 1) - 500) * numCells();
}

// "If we see a battery voltage higher than physics allows, assume charger is pumping in power"
static float chargingVolt()
{
    return ((OCV(0) + 10) * numCells());
}