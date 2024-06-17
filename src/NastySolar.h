#pragma once

#include "RadioLib.h"
#include "configuration.h"

#ifndef RAK_4631
#error This is only for RAK4631 right now
#endif

#ifndef NASTYSOLAR_CUTOFF_MV
#define NASTYSOLAR_CUTOFF_MV 3800 // High enough that RAK4631 deep-sleep current is acceptable
#endif

// While in low-voltage shutdown, how often should the device wake to check the battery
#ifndef NASTYSOLAR_CHECK_MINUTES
#define NASTYSOLAR_CHECK_MINUTES 30
#endif

extern void nastySolarBootCheck();