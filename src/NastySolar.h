#pragma once

#include "RadioLib.h"
#include "configuration.h"

#ifndef RAK_4631
#error This is only for RAK4631 right now
#endif

// How low can the battery run before entering pseudo-sleep
#ifndef NASTYSOLAR_SLEEP_MV
#define NASTYSOLAR_SLEEP_MV 3450
#endif

// How high must the battery recharge before wake is acceptable
// Provides hysteresis - avoid waking until we're sure we've got decent power
#ifndef NASTYSOLAR_RECHARGE_MV
#define NASTYSOLAR_RECHARGE_MV 3700
#endif

// While in low-voltage shutdown, how often should the device wake to check the battery
#ifndef NASTYSOLAR_CHECK_MINUTES
#define NASTYSOLAR_CHECK_MINUTES 30
#endif

extern void nastySolarBootCheck();