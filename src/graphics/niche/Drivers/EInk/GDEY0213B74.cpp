#include "./GDEY0213B74.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

using namespace NicheGraphics::Drivers;

// Map the display controller IC's output to the connected panel
void GDEY0213B74::configScanning()
{
    // "Driver output control"
    sendCommand(0x01);
    sendData((height - 1)); // Scan gates 0 to 249. 250px vertical resolution.
    sendData(0x00);
    sendData(0x00);
}

// Specify which information is used to control the sequence of voltages applied to move the pixels
// - For this display, configUpdateSequence() specifies that a suitable LUT will be loaded from
//   the controller IC's OTP memory, when the update procedure begins.
void GDEY0213B74::configWaveform()
{
    sendCommand(0x3C); // Border waveform:
    sendData(0x05);    // Screen border should follow LUT1 waveform (actively drive pixels white)

    sendCommand(0x21);
    sendData(0x00); // Display update control:
    sendData(0x80); // "Available Source from S8 to S167". Unsure if SSD16XX::bufferOffset is needed here too?

    sendCommand(0x18); // Temperature sensor:
    sendData(0x80);    // Use internal temperature sensor to select an appropriate refresh waveform
}

void GDEY0213B74::configUpdateSequence()
{
    switch (updateType) {
    case FAST:
        sendCommand(0x22); // Set "update sequence"
        sendData(0xFF);    // Will load LUT from OTP memory, Display mode 2 "differential refresh"
        break;

    case FULL:
    default:
        sendCommand(0x22); // Set "update sequence"
        sendData(0xF7);    // Will load LUT from OTP memory
        break;
    }
}

// Once the refresh operation has been started,
// begin periodically polling the display to check for completion, using the normal Meshtastic threading code
// Only used when refresh is "async"
void GDEY0213B74::detachFromUpdate()
{
    switch (updateType) {
    case FAST:
        return beginPolling(50, 300); // At least 300ms for fast refresh
    case FULL:
    default:
        return beginPolling(100, 2500); // At least 2.5 seconds for full refresh
    }
}
#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS