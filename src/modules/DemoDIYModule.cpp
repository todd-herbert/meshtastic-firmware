#include "DemoDIYModule.h"

#ifdef DIYMODULES
#ifdef DIYMODULES_DEMO

// You need to call new() for this in src/modules/Modules.cpp setupModules()
DemoDIYModule *demoDIYModule;

/*
 * Define what we want to save and load from flash.
 *
 * We'll store stuff in this struct, then write / read as needed.
 * If the data is corrupt, or not saved, the defaults set here are used instead.
 */

struct DemoData {
    int32_t exampleNumber = 123;
    char exampleText[20] = "Not Set!";
};

DemoData demoData;

/*
 * -- Constructor --
 *
 * The DIYModule base-class needs two args:
 *   - a name
 *   - a "control style"
 *
 *   In this example, we will interact with the module (in app) by creating a new channel called "Demo"
 *   Commands send to this channel will be passed to your handleSentText() method
 */

DemoDIYModule::DemoDIYModule() : DIYModule("Demo", ControlStyle::OWN_CHANNEL)
{
    // Need to load our stored data from flash
    loadData<DemoData>(&demoData);
}

/*
 * This method is called when we send a command via the app.
 * Call getArg() to handle. Returns a char*
 *
 * NOTE: the char* value changes next time you call getArg
 * If you need this data later, copy it somewhere.
 *
 * If your module uses ControlStyle::OWN_CHANNEL, getArg(0) is the first piece of data
 * If your module uses ControlStyle::BY_NAME, getArg(0) is the module name, getArg(1) is the first data
 * (Specified at the constructor)
 *
 */
void DemoDIYModule::handleSentText(const meshtastic_MeshPacket &mp)
{
    // Get the first part of the command
    char *command = getArg(0);

    // Command: help
    if (stringsMatch(command, "help", false)) // false means "case insensitive"
        exampleHelp();

    // Command: save
    else if (stringsMatch(command, "save", false)) {
        // We need to check the second argument to decide what to save
        char *what = getArg(1);

        // Option: save number
        if (stringsMatch(what, "number"))
            exampleSaveNum(getArg(2));

        // Option: save text
        else if (stringsMatch(what, "text"))
            exampleSaveText(getArg(2, true)); // true - grab all input, don't break at spaces

        // Unknown save option
        else
            sendPhoneFeedback("I didn't understand. What do you want to save? Try help");
    }

    // Command: load
    else if (stringsMatch(command, "load", false))
        exampleLoad();

    // Unknown command
    else
        sendPhoneFeedback("I didn't understand that command.. Try help");
}

// Just for the demo: send a help message to app
void DemoDIYModule::exampleHelp()
{
    sendPhoneFeedback("Just a quick demo, to show off saving config to flash. Available commands are:");
    sendPhoneFeedback("save number <int>");
    sendPhoneFeedback("save text <char>"); // Note: we're only allocating 20 characters for this
    sendPhoneFeedback("load");
}

// Just for the demo: save a number to flash
void DemoDIYModule::exampleSaveNum(char *numAsText)
{
    // Convert the raw input to an int
    int number = atoi(numAsText);

    // Store in flash
    demoData.exampleNumber = number;
    saveData<DemoData>(&demoData);

    sendPhoneFeedback("Saved the number");
}

// Just for the demo: save some text to flash
void DemoDIYModule::exampleSaveText(char *text)
{
    // Make sure the input isn't too long
    if (strlen(text) >= 20) {
        sendPhoneFeedback("Too much text! We only allocated 20 chars in the example..");
        return;
    }

    // Store in flash
    strcpy(demoData.exampleText, text);
    saveData<DemoData>(&demoData);

    sendPhoneFeedback("Saved that text");
}

// Just for the demo: display what we've got in flash
void DemoDIYModule::exampleLoad()
{
    // We don't need to load first, we did this in the constructor
    // Our local version of the struct will match what is in flash

    // sendPhoneFeedback() needs a C-String
    // We can do this the hard way, but I'm lazy so we'll do it with Arduino String

    String ourReply = "";

    ourReply.concat("Number: ");
    ourReply.concat(demoData.exampleNumber);
    ourReply.concat("\n");

    ourReply.concat("Text: ");
    ourReply.concat(demoData.exampleText);

    // Get the C-String version of this, then send back to phone
    sendPhoneFeedback(ourReply.c_str());
}

#endif // DIYMODULES_DEMO
#endif // DIYMODULES