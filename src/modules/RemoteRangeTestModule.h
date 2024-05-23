#pragma once

#include "configuration.h"

#ifdef ENABLE_REMOTE_RANGETEST

#include "SinglePortModule.h"

class RemoteRangetestModule : public SinglePortModule, public concurrency::OSThread
{
  public:
    RemoteRangetestModule();

  protected:
    // New text message from the mesh
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

    // Send a message out over the mesh
    void sendText(const char *message, int channelIndex, uint32_t dest);
    void sendText(const char *message, int channelIndex) { sendText(message, channelIndex, NODENUM_BROADCAST); }
    void sendText(const char *message) { sendText(message, 0, NODENUM_BROADCAST); }
    // Overloaded methods instead of default args, otherwise compiler complains about passing string literals

    // Timer, to run code later
    virtual int32_t runOnce() override;

    // Compare two strings, case insensitive
    static bool stringsMatch(const char *s1, const char *s2, bool caseSensitive = false);

    // Code for remotely starting range test
    void beginRangeTest(uint32_t replyTo, ChannelIndex replyVia);
    bool ranRangeTest = false;
    bool waitingToReboot = false;
};

#endif