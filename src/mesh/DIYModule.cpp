#include "DIYModule.h"

#ifdef DIYMODULES

#include "MeshService.h"
#include "Router.h"

// Allows these static members to be used here for method definitions
char *DIYModule::currentText;
char *DIYModule::requestedArg;

// Container of all classes derived from DIYModule
std::vector<DIYModule *> *DIYModule::diyModules;

// Is the dynamic memory for process arguments currently allocated?
static bool argMemoryAllocated = false;

// Constructor
DIYModule::DIYModule(const char *name, ControlStyle style) : name(name), style(style)
{
    // Add this module instance to the vector
    if (!diyModules)
        diyModules = new std::vector<DIYModule *>();
    diyModules->push_back(this);

    // If controlling with a dedicated channel, create a channel name string, capped at 11 chars
    if (style == OWN_CHANNEL) {
        strncpy(ownChannelName, name, 11);
    }
}

// Intercept a message which the user has sent, if it appears intended for a DIY module
ProcessMessage DIYModule::interceptSentText(meshtastic_MeshPacket &mp, RxSource src)
{
    if (src != RX_SRC_USER) {
        LOG_DEBUG("Intercept: ignoring, not generated by user\n");
        return ProcessMessage::CONTINUE;
    }

    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP) {
        LOG_DEBUG("Intercept: ignoring, only interested in text\n");
        return ProcessMessage::CONTINUE;
    }

    // Grab a copy of the outgoing message
    currentText = new char[mp.decoded.payload.size + 1]; // +1 for null term
    strcpy(currentText, (char *)mp.decoded.payload.bytes);

    // Has a diy module taken ownership of this message?
    bool intercepted = false;

    // For each module
    for (auto i = diyModules->begin(); i != diyModules->end(); ++i) {
        auto &cm = **i;

        // Decide if current module wants this input
        bool moduleWantsText = false;
        if (cm.style == BY_NAME && stringsMatch(getArg(0), cm.name, false)) // Global command
            moduleWantsText = true;
        else if (cm.style == OWN_CHANNEL && isFromChannel(mp, cm.ownChannelName)) // Own channel
            moduleWantsText = true;

        // If module wants text
        if (moduleWantsText) {
            // Let the module handle it
            cm.handleSentText(mp);

            // Cancel the message, and lie to the phone that it was sent successfully
            service.cancelSending(mp.id);
            meshtastic_QueueStatus qs = router->getQueueStatus();
            ErrorCode r = service.sendQueueStatusToPhone(qs, meshtastic_Routing_Error_NONE, mp.id);
            if (r != ERRNO_OK)
                LOG_DEBUG("DIYModule can't send status to phone");

            intercepted = true; // DIY module has consumed this message - don't send it to the mesh
            break;
        }
    }

    delete[] currentText; // Delete the copy we made of the original outgoing message

    // Free memory, which might have been allocated by getArg()
    if (argMemoryAllocated) {
        delete[] requestedArg;
        argMemoryAllocated = false;
    }

    if (intercepted)
        spoofACK(mp);

    return intercepted ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
}

// Send info back to user, appearing as a text message in a mesh channel
void DIYModule::sendPhoneFeedback(const char *text, const char *channelName)
{
    // Determine which channel this message should appear in
    meshtastic_Channel feedbackChannel;
    if (style == BY_NAME)
        feedbackChannel = channels.getByName(channelName);
    else                                                      // OWN_CHANNEL
        feedbackChannel = channels.getByName(ownChannelName); // module name

    // Load the info into a new packet
    meshtastic_MeshPacket *feedback = router->allocForSending();
    feedback->to = NODENUM_BROADCAST;
    feedback->from = NODENUM_BROADCAST;
    feedback->channel = feedbackChannel.index;
    feedback->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    feedback->decoded.payload.size = strlen(text);
    memcpy(feedback->decoded.payload.bytes, text, feedback->decoded.payload.size);

    // Send the new packet off to the phone
    LOG_DEBUG("Sent feedback to phone: \"%s\"\n", text);
    service.sendToPhone(feedback);
}

void DIYModule::spoofACK(meshtastic_MeshPacket &packetToAck)
{
    // Create a routing message
    meshtastic_Routing r = meshtastic_Routing_init_default;
    r.error_reason = meshtastic_Routing_Error::meshtastic_Routing_Error_NONE;
    r.which_variant = meshtastic_Routing_error_reason_tag;

    // Load the routing message into a packet, and set the other details
    meshtastic_MeshPacket *p = router->allocForSending();
    p->decoded.portnum = meshtastic_PortNum_ROUTING_APP;
    p->decoded.payload.size =
        pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes), &meshtastic_Routing_msg, &r);
    p->priority = meshtastic_MeshPacket_Priority_ACK;
    p->hop_limit = 0;
    p->to = myNodeInfo.my_node_num;
    p->decoded.request_id = packetToAck.id;
    p->channel = packetToAck.channel;

    // Send the ACK packet to phone
    LOG_DEBUG("Sending ACK for intercepted message\n");
    service.sendToPhone(p);
}

// Check if a mesh channel exists
bool DIYModule::channelExists(const char *channelName)
{
    int8_t allegedIndex = channels.getByName(channelName).index;
    return strcmp(channels.getByIndex(allegedIndex).settings.name, channelName) == 0; // Did getByName() find the correct channel?
}

// Check which channel a message was intercepted from
bool DIYModule::isFromChannel(const meshtastic_MeshPacket &mp, const char *channelName)
{
    return strcmp(channelName, channels.getByIndex(mp.channel).settings.name) == 0; // Compare names only
}

// Check if message was intercepted from the public longfast channel
bool DIYModule::isFromPublicChannel(const meshtastic_MeshPacket &mp)
{
    return isFromChannel(mp, "");
}

// Get the name of the channel to which the message was sent
const char *DIYModule::getChannelName(const meshtastic_MeshPacket &mp)
{
    return channels.getByIndex(mp.channel).settings.name;
}

// Parse raw text as a true/false value
bool DIYModule::parseBool(const char *raw)
{
    return stringsMatch(raw, "true", false);
}

// Check if two strings match. Option to ignore the case when comparing
bool DIYModule::stringsMatch(const char *s1, const char *s2, bool caseSensitive)
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

// Parse commands / data from an intercepted outgoing text message
// Use inside handleSentText()
// If module's ControlStyle is BY_NAME, messages are intercepted if getArg(0) matches the module name
// If module's ControlStyle is OWN_CHANNEL, messages are intercepted from a mesh channel with the same name as the module
// In this case, getArg(0) is the first command / piece of data
// args are separated by space
// To get a longer string, call getArg(num, true), where "num" is the argument to start from.
// All text between this arg, and the end of the message, will be grabbed.
char *DIYModule::getArg(uint8_t index, bool untilEnd)
{
    // Free the existing memory, if still in use
    if (argMemoryAllocated) {
        delete[] requestedArg;
        argMemoryAllocated = false;
    }

    // Handle an empty string separately - save headaches
    if (strlen(currentText) == 0) {
        requestedArg = new char[1];
        argMemoryAllocated = true;
        requestedArg[0] = '\0';
        return requestedArg;
    }

    // Find start and end index of our substring
    constexpr char delimiter = ' ';
    uint8_t delimiterCount = 0;
    uint8_t start = 0;
    uint8_t end = 0;

    for (uint16_t i = 0;; i++) {
        // Null term
        if (currentText[i] == '\0') {
            end = i - 1; // Exclude the null term here, just to stay consistent
            break;       // Found the end
        }

        // Delimiter
        if (currentText[i] == delimiter) {
            delimiterCount++;
            if (delimiterCount == index)
                start = i + 1;

            if (delimiterCount == index + 1 && !untilEnd) {
                end = i - 1;
                break; // Found the end
            }
        }
    }

    // If we found an argument and the requested index
    if (delimiterCount >= index) {
        uint8_t substringLength = (end - start) + 1;  // +1 "arrays start at zero"
        requestedArg = new char[substringLength + 1]; // + 1 for terminator
        argMemoryAllocated = true;
        strncpy(requestedArg, currentText + start, substringLength); // Copy the substring (no null term)
        requestedArg[substringLength] = '\0';                        // Append null term
    }
    // If the index exceed the number of arguments parsed
    else {
        requestedArg = new char[1];
        requestedArg[0] = '\0';
    }

    return requestedArg; // Really just for convenience. We always return requestedArg
}

// Used to verify integrity of data / settings saved to flash
uint32_t DIYModule::getDataHash(void *data, uint32_t size)
{
    uint32_t hash = 0;

    // Sum all bytes of the image buffer together
    for (uint32_t i = 0; i < size; i++)
        hash += ((uint8_t *)data)[i];

    return hash;
}

#endif // DIYMODULES