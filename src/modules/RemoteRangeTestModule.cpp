#include "RemoteRangeTestModule.h"

#ifdef ENABLE_REMOTE_RANGETEST

#include "MeshService.h"
#include "Router.h"

static const int durationMinutes = REMOTE_RANGETEST_DURATION; // How long to run the range test
static const int intervalMinutes = REMOTE_RANGETEST_INTERVAL; // How often can range tests run
static char triggerWord[] = REMOTE_RANGETEST_TRIGGER;
uint8_t channelIndex = REMOTE_RANGETEST_LISTEN_CHANINDEX;

// Do at startup
RemoteRangetestModule::RemoteRangetestModule()
    : SinglePortModule("RemoteRangeTest", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("RemoteRangeTest")
{
    // If range test enabled
    if (moduleConfig.range_test.enabled) {
        ranRangeTest = true;                         // Impose the rate-limiting interval
        setInterval(durationMinutes * MS_IN_MINUTE); // Set timer to disable range test
        LOG_INFO("Will disable range test in %i minutes\n", durationMinutes);
    } else
        disable(); // No timer
}

// Do something when text is received
ProcessMessage RemoteRangetestModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Grab info from the received protobuf
    char *text = (char *)mp.decoded.payload.bytes;

    // If we like this message, start the test
    if (mp.channel == channelIndex && stringsMatch(text, triggerWord)) {
        LOG_INFO("User asked for a rangetest\n");
        beginRangeTest(mp.from, channelIndex);
        return ProcessMessage::STOP; // Ignore this message. No rebroadcast, etc
    }

    return ProcessMessage::CONTINUE; // We weren't interested in this message, treat it as nomal
}

// Check if it's appropriate, then run range test
void RemoteRangetestModule::beginRangeTest(uint32_t informNode, ChannelIndex informViaChannel)
{
    // Abort: if already running
    if (moduleConfig.range_test.enabled) {
        LOG_INFO("Range test already running\n");
        sendText("Range test is already running", informViaChannel, informNode);
        return;
    }

    // Abort: if there was a previous test recently
    if (ranRangeTest && (millis() < intervalMinutes * MS_IN_MINUTE)) {
        LOG_INFO("Too soon for another range test\n");
        String reply = "Too soon for another range test. ";
        reply.concat("Try again in ");
        reply.concat(intervalMinutes - (millis() / MS_IN_MINUTE));
        reply.concat(" minutes.");
        sendText(reply.c_str(), informViaChannel, informNode);
        return;
    }

    // Looks okay to start range test
    // Set the module config, then reboot

    LOG_INFO("Looks okay: enabling range test\n");
    String reply = "About to begin the range test. ";
    reply.concat("Please enable the range test module in settings. ");
    reply.concat("The test will run for ");
    reply.concat(durationMinutes);
    reply.concat(" minutes.");
    sendText(reply.c_str(), informViaChannel, informNode);
    moduleConfig.range_test.enabled = true;   // Enable the range test module
    nodeDB->saveToDisk(SEGMENT_MODULECONFIG); // Save this changed config to disk

    // Set the
    waitingToReboot = true;
    setIntervalFromNow(15 * 1000UL);
    OSThread::enabled = true;
}

// Timer: when we need to wait in the background
int32_t RemoteRangetestModule::runOnce()
{
    // If we've enabled the range test module, and are just waiting to reboot to apply the changes
    if (waitingToReboot) {
        // Reboot (platform specific)
#if defined(ARCH_ESP32)
        ESP.restart();
#elif defined(ARCH_NRF52)
        NVIC_SystemReset();
#endif
    }

    // If the range test module is running, and is due to be disabled
    if (moduleConfig.range_test.enabled) {
        LOG_INFO("Time's up! Disabling range test\n");

        moduleConfig.range_test.enabled = false;
        nodeDB->saveToDisk(SEGMENT_MODULECONFIG); // Save this changed config to disk
        return disable();                         // stop the timer
    }

    return disable(); // We shouldn't be able to reach this point..
}

// Send a text message over the mesh. "Borrowed" from canned message module
void RemoteRangetestModule::sendText(const char *message, int channelIndex, uint32_t dest)
{
    meshtastic_MeshPacket *p = router->allocForSending();
    p->to = dest;
    p->channel = channelIndex;
    p->want_ack = true;
    p->decoded.payload.size = strlen(message);
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);

    LOG_DEBUG("Sending message id=%d, dest=%x, msg=%.*s\n", p->id, p->to, p->decoded.payload.size, p->decoded.payload.bytes);

    service.sendToMesh(p, RX_SRC_LOCAL, true);
}

// Check if two strings match. Option to ignore the case when comparing
bool RemoteRangetestModule::stringsMatch(const char *s1, const char *s2, bool caseSensitive)
{
    // If different length, no match
    if (strlen(s1) != strlen(s2))
        return false;

    // Compare character by character (possible case-insensitive)
    for (uint16_t i = 0; i <= strlen(s1); i++) {
        if (caseSensitive && s1[i] != s2[i])
            return false;
        if (!caseSensitive && tolower(s1[i]) != tolower(s2[i]))
            return false;
    }

    return true;
}

#endif