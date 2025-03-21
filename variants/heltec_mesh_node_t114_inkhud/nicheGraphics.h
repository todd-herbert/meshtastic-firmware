#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

// InkHUD-specific components
// ---------------------------
#include "graphics/niche/InkHUD/InkHUD.h"

// Applets
#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Heard/HeardApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Positions/PositionsApplet.h"
#include "graphics/niche/InkHUD/Applets/User/RecentsList/RecentsListApplet.h"
#include "graphics/niche/InkHUD/Applets/User/ThreadedMessage/ThreadedMessageApplet.h"

// #include "graphics/niche/InkHUD/Applets/Examples/BasicExample/BasicExampleApplet.h"
// #include "graphics/niche/InkHUD/Applets/Examples/NewMsgExample/NewMsgExampleApplet.h"

// Shared NicheGraphics components
// --------------------------------
#include "graphics/niche/Drivers/EInk/HINK_E042A87.h"
#include "graphics/niche/Inputs/TwoButton.h"

#include "graphics/niche/Fonts/FreeSans6pt7b.h"
#include "graphics/niche/Fonts/FreeSans6pt8bCyrillic.h"
#include <Fonts/FreeSans9pt7b.h>

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    // SPI
    // -----------------------------

    // For NRF52 platforms, SPI pins are defined in variant.h, not passed to begin()
    SPIClass *inkSPI = &SPI1;
    inkSPI->begin();

    // Driver
    // -----------------------------

    // Use E-Ink driver
    Drivers::EInk *driver = new Drivers::HINK_E042A87;
    driver->begin(inkSPI, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    // InkHUD
    // ----------------------------

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();

    // Set the driver
    inkhud->setDriver(driver);

    // Set how many FAST updates per FULL update
    // Set how unhealthy additional FAST updates beyond this number are
    inkhud->setDisplayResilience(15, 2);
#warning Display resilience has not yet been determined

    // Prepare fonts
    InkHUD::Applet::fontLarge = InkHUD::AppletFont(FreeSans9pt7b);
    InkHUD::Applet::fontSmall = InkHUD::AppletFont(FreeSans6pt7b);

    /*
    // Font localization demo: Cyrillic
    InkHUD::Applet::fontSmall = InkHUD::AppletFont(FreeSans6pt8bCyrillic);
    InkHUD::Applet::fontSmall.addSubstitutionsWin1251();
    */

    // Init settings, and customize defaults
    // Values ignored individually if found saved to flash
    inkhud->persistence->settings.rotation = 0;
    inkhud->persistence->settings.userTiles.count = 2;
    inkhud->persistence->settings.optionalFeatures.batteryIcon = true; // Device might have a battery

    // Pick applets
    // Note: order of applets determines priority of "auto-show" feature
    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet);
    inkhud->addApplet("DMs", new InkHUD::DMApplet, true, false, 3);                       // Default on tile 3
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0), true, false, 2); // Default on tile 2
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true, false, 1);      // Default on tile 1
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet, true, false, 0); // Default on tile 0
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true);                        // Background
    // inkhud->addApplet("Basic", new InkHUD::BasicExampleApplet);
    // inkhud->addApplet("NewMsg", new InkHUD::NewMsgExampleApplet);

    // Start running InkHUD
    inkhud->begin();

    // Buttons
    // --------------------------

    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance(); // Shared NicheGraphics component

    // Setup the main user button
    buttons->setWiring(0, BUTTON_PIN);
    buttons->setHandlerShortPress(0, []() { InkHUD::InkHUD::getInstance()->shortpress(); });
    buttons->setHandlerLongPress(0, []() { InkHUD::InkHUD::getInstance()->longpress(); });

    buttons->start();

    LOG_INFO("InkHUD started");
}

#endif