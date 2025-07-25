#pragma once

#include "configuration.h"

#include "detect/ScanI2C.h"
#include "mesh/generated/meshtastic/config.pb.h"
#include <OLEDDisplay.h>
#include <functional>
#include <string>
#include <vector>

#define getStringCenteredX(s) ((SCREEN_WIDTH - display->getStringWidth(s)) / 2)
namespace graphics
{
enum notificationTypeEnum { none, text_banner, selection_picker, node_picker, number_picker };

struct BannerOverlayOptions {
    const char *message;
    uint32_t durationMs = 30000;
    const char **optionsArrayPtr = nullptr;
    const int *optionsEnumPtr = nullptr;
    uint8_t optionsCount = 0;
    std::function<void(int)> bannerCallback = nullptr;
    int8_t InitialSelected = 0;
    notificationTypeEnum notificationType = notificationTypeEnum::text_banner;
};
} // namespace graphics

bool shouldWakeOnReceivedMessage();

#if !HAS_SCREEN
#include "power.h"
namespace graphics
{
// Noop class for boards without screen.
class Screen
{
  public:
    enum FrameFocus : uint8_t {
        FOCUS_DEFAULT,  // No specific frame
        FOCUS_PRESERVE, // Return to the previous frame
        FOCUS_FAULT,
        FOCUS_TEXTMESSAGE,
        FOCUS_MODULE, // Note: target module should call requestFocus(), otherwise no info about which module to focus
        FOCUS_CLOCK,
        FOCUS_SYSTEM,
    };

    explicit Screen(ScanI2C::DeviceAddress, meshtastic_Config_DisplayConfig_OledType, OLEDDISPLAY_GEOMETRY);
    void onPress() {}
    void setup() {}
    void setOn(bool) {}
    void doDeepSleep() {}
    void forceDisplay(bool forceUiUpdate = false) {}
    void startFirmwareUpdateScreen() {}
    void increaseBrightness() {}
    void decreaseBrightness() {}
    void setFunctionSymbol(std::string) {}
    void removeFunctionSymbol(std::string) {}
    void startAlert(const char *) {}
    void showSimpleBanner(const char *message, uint32_t durationMs = 0) {}
    void showOverlayBanner(BannerOverlayOptions) {}
    void setFrames(FrameFocus focus) {}
    void endAlert() {}
};
} // namespace graphics
#else
#include <cstring>

#include <OLEDDisplayUi.h>

#include "../configuration.h"
#include "gps/GeoCoord.h"
#include "graphics/ScreenFonts.h"

#ifdef USE_ST7567
#include <ST7567Wire.h>
#elif defined(USE_SH1106) || defined(USE_SH1107) || defined(USE_SH1107_128_64)
#include <SH1106Wire.h>
#elif defined(USE_SSD1306)
#include <SSD1306Wire.h>
#elif defined(USE_ST7789)
#include <ST7789Spi.h>
#else
// the SH1106/SSD1306 variant is auto-detected
#include <AutoOLEDWire.h>
#endif

#include "EInkDisplay2.h"
#include "EInkDynamicDisplay.h"
#include "PointStruct.h"
#include "TFTDisplay.h"
#include "TypedQueue.h"
#include "commands.h"
#include "concurrency/LockGuard.h"
#include "concurrency/OSThread.h"
#include "graphics/draw/MenuHandler.h"
#include "input/InputBroker.h"
#include "mesh/MeshModule.h"
#include "modules/AdminModule.h"
#include "power.h"
#include <string>
#include <vector>

// 0 to 255, though particular variants might define different defaults
#ifndef BRIGHTNESS_DEFAULT
#define BRIGHTNESS_DEFAULT 150
#endif

// Meters to feet conversion
#ifndef METERS_TO_FEET
#define METERS_TO_FEET 3.28
#endif

// Feet to miles conversion
#ifndef MILES_TO_FEET
#define MILES_TO_FEET 5280
#endif

// Intuitive colors. E-Ink display is inverted from OLED(?)
#define EINK_BLACK OLEDDISPLAY_COLOR::WHITE
#define EINK_WHITE OLEDDISPLAY_COLOR::BLACK

// Base segment dimensions for T-Watch segmented display
#define SEGMENT_WIDTH 16
#define SEGMENT_HEIGHT 4

extern bool wake_on_received_message;

/// Convert an integer GPS coords to a floating point
#define DegD(i) (i * 1e-7)
extern bool hasUnreadMessage;
namespace
{
/// A basic 2D point class for drawing
class Point
{
  public:
    float x, y;

    Point(float _x, float _y) : x(_x), y(_y) {}

    /// Apply a rotation around zero (standard rotation matrix math)
    void rotate(float radian)
    {
        float cos = cosf(radian), sin = sinf(radian);
        float rx = x * cos + y * sin, ry = -x * sin + y * cos;

        x = rx;
        y = ry;
    }

    void translate(int16_t dx, int dy)
    {
        x += dx;
        y += dy;
    }

    void scale(float f)
    {
        // We use -f here to counter the flip that happens
        // on the y axis when drawing and rotating on screen
        x *= f;
        y *= -f;
    }
};

} // namespace

namespace graphics
{

// Forward declarations
class Screen;

/// Handles gathering and displaying debug information.
class DebugInfo
{
  public:
    DebugInfo(const DebugInfo &) = delete;
    DebugInfo &operator=(const DebugInfo &) = delete;

  private:
    friend Screen;

    DebugInfo() {}

    /// Renders the debug screen.
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    void drawFrameSettings(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    void drawFrameWiFi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    /// Protects all of internal state.
    concurrency::Lock lock;
};

/**
 * @brief This class deals with showing things on the screen of the device.
 *
 * @details Other than setup(), this class is thread-safe as long as drawFrame is not called
 *          multiple times simultaneously. All state-changing calls are queued and executed
 *          when the main loop calls us.
 */
class Screen : public concurrency::OSThread
{
    CallbackObserver<Screen, const meshtastic::Status *> powerStatusObserver =
        CallbackObserver<Screen, const meshtastic::Status *>(this, &Screen::handleStatusUpdate);
    CallbackObserver<Screen, const meshtastic::Status *> gpsStatusObserver =
        CallbackObserver<Screen, const meshtastic::Status *>(this, &Screen::handleStatusUpdate);
    CallbackObserver<Screen, const meshtastic::Status *> nodeStatusObserver =
        CallbackObserver<Screen, const meshtastic::Status *>(this, &Screen::handleStatusUpdate);
    CallbackObserver<Screen, const meshtastic_MeshPacket *> textMessageObserver =
        CallbackObserver<Screen, const meshtastic_MeshPacket *>(this, &Screen::handleTextMessage);
    CallbackObserver<Screen, const UIFrameEvent *> uiFrameEventObserver =
        CallbackObserver<Screen, const UIFrameEvent *>(this, &Screen::handleUIFrameEvent); // Sent by Mesh Modules
    CallbackObserver<Screen, const InputEvent *> inputObserver =
        CallbackObserver<Screen, const InputEvent *>(this, &Screen::handleInputEvent);
    CallbackObserver<Screen, AdminModule_ObserverData *> adminMessageObserver =
        CallbackObserver<Screen, AdminModule_ObserverData *>(this, &Screen::handleAdminMessage);

  public:
    OLEDDisplay *getDisplayDevice() { return dispdev; }
    explicit Screen(ScanI2C::DeviceAddress, meshtastic_Config_DisplayConfig_OledType, OLEDDISPLAY_GEOMETRY);
    size_t frameCount = 0; // Total number of active frames
    ~Screen();

    // Which frame we want to be displayed, after we regen the frameset by calling setFrames
    enum FrameFocus : uint8_t {
        FOCUS_DEFAULT,  // No specific frame
        FOCUS_PRESERVE, // Return to the previous frame
        FOCUS_FAULT,
        FOCUS_TEXTMESSAGE,
        FOCUS_MODULE, // Note: target module should call requestFocus(), otherwise no info about which module to focus
        FOCUS_CLOCK,
        FOCUS_SYSTEM,
    };

    // Regenerate the normal set of frames, focusing a specific frame if requested
    // Call when a frame should be added / removed, or custom frames should be cleared
    void setFrames(FrameFocus focus = FOCUS_DEFAULT);

    std::vector<const uint8_t *> indicatorIcons; // Per-frame custom icon pointers
    Screen(const Screen &) = delete;
    Screen &operator=(const Screen &) = delete;

    ScanI2C::DeviceAddress address_found;
    meshtastic_Config_DisplayConfig_OledType model;
    OLEDDISPLAY_GEOMETRY geometry;

    bool isOverlayBannerShowing();

    // Stores the last 4 of our hardware ID, to make finding the device for pairing easier
    // FIXME: Needs refactoring and getMacAddr needs to be moved to a utility class
    char ourId[5];

    /// Initializes the UI, turns on the display, starts showing boot screen.
    //
    // Not thread safe - must be called before any other methods are called.
    void setup();

    /// Turns the screen on/off. Optionally, pass a custom screensaver frame for E-Ink
    void setOn(bool on, FrameCallback einkScreensaver = NULL)
    {
        if (!on)
            // We handle off commands immediately, because they might be called because the CPU is shutting down
            handleSetOn(false, einkScreensaver);
        else
            enqueueCmd(ScreenCmd{.cmd = Cmd::SET_ON});
    }

    /**
     * Prepare the display for the unit going to the lowest power mode possible.  Most screens will just
     * poweroff, but eink screens will show a "I'm sleeping" graphic, possibly with a QR code
     */
    void doDeepSleep();

    void blink();

    // Draw north
    float estimatedHeading(double lat, double lon);

    /// Handle button press, trackball or swipe action)
    void onPress() { enqueueCmd(ScreenCmd{.cmd = Cmd::ON_PRESS}); }
    void showPrevFrame() { enqueueCmd(ScreenCmd{.cmd = Cmd::SHOW_PREV_FRAME}); }
    void showNextFrame() { enqueueCmd(ScreenCmd{.cmd = Cmd::SHOW_NEXT_FRAME}); }

    // generic alert start
    void startAlert(FrameCallback _alertFrame)
    {
        alertFrame = _alertFrame;
        ScreenCmd cmd;
        cmd.cmd = Cmd::START_ALERT_FRAME;
        enqueueCmd(cmd);
    }

    void startAlert(const char *_alertMessage)
    {
        startAlert([_alertMessage](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
            uint16_t x_offset = display->width() / 2;
            display->setTextAlignment(TEXT_ALIGN_CENTER);
            display->setFont(FONT_MEDIUM);
            display->drawString(x_offset + x, 26 + y, _alertMessage);
        });
    }

    void endAlert()
    {
        ScreenCmd cmd;
        cmd.cmd = Cmd::STOP_ALERT_FRAME;
        enqueueCmd(cmd);
    }

    void showSimpleBanner(const char *message, uint32_t durationMs = 0);
    void showOverlayBanner(BannerOverlayOptions);

    void showNodePicker(const char *message, uint32_t durationMs, std::function<void(uint32_t)> bannerCallback);
    void showNumberPicker(const char *message, uint32_t durationMs, uint8_t digits, std::function<void(uint32_t)> bannerCallback);

    void requestMenu(graphics::menuHandler::screenMenus menuToShow)
    {
        graphics::menuHandler::menuQueue = menuToShow;
        runNow();
    }

    void startFirmwareUpdateScreen()
    {
        ScreenCmd cmd;
        cmd.cmd = Cmd::START_FIRMWARE_UPDATE_SCREEN;
        enqueueCmd(cmd);
    }

    // Function to allow the AccelerometerThread to set the heading if a sensor provides it
    // Mutex needed?
    void setHeading(long _heading)
    {
        hasCompass = true;
        compassHeading = fmod(_heading, 360);
    }

    bool hasHeading() { return hasCompass; }

    long getHeading() { return compassHeading; }

    void setEndCalibration(uint32_t _endCalibrationAt) { endCalibrationAt = _endCalibrationAt; }
    uint32_t getEndCalibration() { return endCalibrationAt; }

    // functions for display brightness
    void increaseBrightness();
    void decreaseBrightness();

    void setFunctionSymbol(std::string sym);
    void removeFunctionSymbol(std::string sym);

    /// Stops showing the boot screen.
    void stopBootScreen() { enqueueCmd(ScreenCmd{.cmd = Cmd::STOP_BOOT_SCREEN}); }

    void runNow()
    {
        setFastFramerate();
        enqueueCmd(ScreenCmd{.cmd = Cmd::NOOP});
    }

    /// Overrides the default utf8 character conversion, to replace empty space with question marks
    static char customFontTableLookup(const uint8_t ch)
    {
        // UTF-8 to font table index converter
        // Code from http://playground.arduino.cc/Main/Utf8ascii
        static uint8_t LASTCHAR;
        static bool SKIPREST; // Only display a single unconvertable-character symbol per sequence of unconvertable characters

        if (ch < 128) { // Standard ASCII-set 0..0x7F handling
            LASTCHAR = 0;
            SKIPREST = false;
            return ch;
        }

        uint8_t last = LASTCHAR; // get last char
        LASTCHAR = ch;

        switch (last) {
        case 0xC2: {
            SKIPREST = false;
            return (uint8_t)ch;
        }

        case 0xC3: {
            SKIPREST = false;
            return (uint8_t)(ch | 0xC0);
        }
        }

        // We want to strip out prefix chars for two-byte char formats
        if (ch == 0xC2 || ch == 0xC3)
            return (uint8_t)0;

#if defined(OLED_PL)

        switch (last) {
        case 0xC3: {

            if (ch == 147)
                return (uint8_t)(ch); // Ó
            else if (ch == 179)
                return (uint8_t)(148); // ó
            else
                return (uint8_t)(ch | 0xC0);
            break;
        }

        case 0xC4: {
            SKIPREST = false;
            return (uint8_t)(ch);
        }

        case 0xC5: {
            SKIPREST = false;
            if (ch == 132)
                return (uint8_t)(136); // ń
            else if (ch == 186)
                return (uint8_t)(137); // ź
            else
                return (uint8_t)(ch);
            break;
        }
        }

        // We want to strip out prefix chars for two-byte char formats
        if (ch == 0xC2 || ch == 0xC3 || ch == 0xC4 || ch == 0xC5)
            return (uint8_t)0;

#endif

#if defined(OLED_UA) || defined(OLED_RU)

        switch (last) {
        case 0xC3: {
            SKIPREST = false;
            return (uint8_t)(ch | 0xC0);
        }
        // map UTF-8 cyrillic chars to it Windows-1251 (CP-1251) ASCII codes
        // note: in this case we must use compatible font - provided ArialMT_Plain_10/16/24 by 'ThingPulse/esp8266-oled-ssd1306'
        // library have empty chars for non-latin ASCII symbols
        case 0xD0: {
            SKIPREST = false;
            if (ch == 132)
                return (uint8_t)(170); // Є
            if (ch == 134)
                return (uint8_t)(178); // І
            if (ch == 135)
                return (uint8_t)(175); // Ї
            if (ch == 129)
                return (uint8_t)(168); // Ё
            if (ch > 143 && ch < 192)
                return (uint8_t)(ch + 48);
            break;
        }
        case 0xD1: {
            SKIPREST = false;
            if (ch == 148)
                return (uint8_t)(186); // є
            if (ch == 150)
                return (uint8_t)(179); // і
            if (ch == 151)
                return (uint8_t)(191); // ї
            if (ch == 145)
                return (uint8_t)(184); // ё
            if (ch > 127 && ch < 144)
                return (uint8_t)(ch + 112);
            break;
        }
        case 0xD2: {
            SKIPREST = false;
            if (ch == 144)
                return (uint8_t)(165); // Ґ
            if (ch == 145)
                return (uint8_t)(180); // ґ
            break;
        }
        }

        // We want to strip out prefix chars for two-byte char formats
        if (ch == 0xC2 || ch == 0xC3 || ch == 0x82 || ch == 0xD0 || ch == 0xD1)
            return (uint8_t)0;

#endif

#if defined(OLED_CS)

        switch (last) {
        case 0xC2: {
            SKIPREST = false;
            return (uint8_t)ch;
        }

        case 0xC3: {
            SKIPREST = false;
            return (uint8_t)(ch | 0xC0);
        }

        case 0xC4: {
            SKIPREST = false;
            if (ch == 140)
                return (uint8_t)(129); // Č
            if (ch == 141)
                return (uint8_t)(138); // č
            if (ch == 142)
                return (uint8_t)(130); // Ď
            if (ch == 143)
                return (uint8_t)(139); // ď
            if (ch == 154)
                return (uint8_t)(131); // Ě
            if (ch == 155)
                return (uint8_t)(140); // ě
            // Slovak specific glyphs
            if (ch == 185)
                return (uint8_t)(147); // Ĺ
            if (ch == 186)
                return (uint8_t)(148); // ĺ
            if (ch == 189)
                return (uint8_t)(149); // Ľ
            if (ch == 190)
                return (uint8_t)(150); // ľ
            break;
        }

        case 0xC5: {
            SKIPREST = false;
            if (ch == 135)
                return (uint8_t)(132); // Ň
            if (ch == 136)
                return (uint8_t)(141); // ň
            if (ch == 152)
                return (uint8_t)(133); // Ř
            if (ch == 153)
                return (uint8_t)(142); // ř
            if (ch == 160)
                return (uint8_t)(134); // Š
            if (ch == 161)
                return (uint8_t)(143); // š
            if (ch == 164)
                return (uint8_t)(135); // Ť
            if (ch == 165)
                return (uint8_t)(144); // ť
            if (ch == 174)
                return (uint8_t)(136); // Ů
            if (ch == 175)
                return (uint8_t)(145); // ů
            if (ch == 189)
                return (uint8_t)(137); // Ž
            if (ch == 190)
                return (uint8_t)(146); // ž
            // Slovak specific glyphs
            if (ch == 148)
                return (uint8_t)(151); // Ŕ
            if (ch == 149)
                return (uint8_t)(152); // ŕ
            break;
        }
        }

        // We want to strip out prefix chars for two-byte char formats
        if (ch == 0xC2 || ch == 0xC3 || ch == 0xC4 || ch == 0xC5)
            return (uint8_t)0;

#endif

        // If we already returned an unconvertable-character symbol for this unconvertable-character sequence, return NULs for the
        // rest of it
        if (SKIPREST)
            return (uint8_t)0;
        SKIPREST = true;

        return (uint8_t)191; // otherwise: return ¿ if character can't be converted (note that the font map we're using doesn't
                             // stick to standard EASCII codes)
    }

    /// Returns a handle to the DebugInfo screen.
    //
    // Use this handle to set things like battery status, user count, GPS status, etc.
    DebugInfo *debug_info() { return &debugInfo; }

    // Handle observer events
    int handleStatusUpdate(const meshtastic::Status *arg);
    int handleTextMessage(const meshtastic_MeshPacket *arg);
    int handleUIFrameEvent(const UIFrameEvent *arg);
    int handleInputEvent(const InputEvent *arg);
    int handleAdminMessage(AdminModule_ObserverData *arg);

    /// Used to force (super slow) eink displays to draw critical frames
    void forceDisplay(bool forceUiUpdate = false);

    /// Draws our SSL cert screen during boot (called from WebServer)
    void setSSLFrames();

    // Dismiss the currently focussed frame, if possible (e.g. text message, waypoint)
    void dismissCurrentFrame();

#ifdef USE_EINK
    /// Draw an image to remain on E-Ink display after screen off
    void setScreensaverFrames(FrameCallback einkScreensaver = NULL);
#endif

  protected:
    /// Updates the UI.
    //
    // Called periodically from the main loop.
    int32_t runOnce() final;

    bool isAUTOOled = false;

    // Screen dimensions (for convenience)
    // Defined during Screen::setup
    uint16_t displayWidth = 0;
    uint16_t displayHeight = 0;

  private:
    FrameCallback alertFrames[1];
    struct ScreenCmd {
        Cmd cmd;
        union {
            uint32_t bluetooth_pin;
            char *print_text;
        };
    };

    /// Enques given command item to be processed by main loop().
    bool enqueueCmd(const ScreenCmd &cmd)
    {
        if (!useDisplay)
            return false; // not enqueued if our display is not in use
        else {
            bool success = cmdQueue.enqueue(cmd, 0);
            enabled = true; // handle ASAP (we are the registered reader for cmdQueue, but might have been disabled)
            return success;
        }
    }

    // Implementations of various commands, called from doTask().
    void handleSetOn(bool on, FrameCallback einkScreensaver = NULL);
    void handleOnPress();
    void handleShowNextFrame();
    void handleShowPrevFrame();
    void handleStartFirmwareUpdateScreen();

    // Info collected by setFrames method.
    // Index location of specific frames.
    // - Used to apply the FrameFocus parameter of setFrames
    // - Used to dismiss the currently shown frame (txt; waypoint) by CardKB combo
    struct FramesetInfo {
        struct FramePositions {
            uint8_t fault = 255;
            uint8_t waypoint = 255;
            uint8_t focusedModule = 255;
            uint8_t log = 255;
            uint8_t settings = 255;
            uint8_t wifi = 255;
            uint8_t deviceFocused = 255;
            uint8_t memory = 255;
            uint8_t gps = 255;
            uint8_t home = 255;
            uint8_t textMessage = 255;
            uint8_t nodelist = 255;
            uint8_t nodelist_lastheard = 255;
            uint8_t nodelist_hopsignal = 255;
            uint8_t nodelist_distance = 255;
            uint8_t nodelist_bearings = 255;
            uint8_t clock = 255;
            uint8_t firstFavorite = 255;
            uint8_t lastFavorite = 255;
            uint8_t lora = 255;
        } positions;

        uint8_t frameCount = 0;
    } framesetInfo;

    struct DismissedFrames {
        bool textMessage = false;
        bool waypoint = false;
        bool wifi = false;
        bool memory = false;
    } dismissedFrames;

    /// Try to start drawing ASAP
    void setFastFramerate();

    // Sets frame up for immediate drawing
    void setFrameImmediateDraw(FrameCallback *drawFrames);

    /// callback for current alert frame
    FrameCallback alertFrame;

    /// Queue of commands to execute in doTask.
    TypedQueue<ScreenCmd> cmdQueue;
    /// Whether we are using a display
    bool useDisplay = false;
    /// Whether the display is currently powered
    bool screenOn = false;
    // Whether we are showing the regular screen (as opposed to booth screen or
    // Bluetooth PIN screen)
    bool showingNormalScreen = false;

    // Implementation to Adjust Brightness
    uint8_t brightness = BRIGHTNESS_DEFAULT; // H = 254, MH = 192, ML = 130 L = 103

    bool hasCompass = false;
    float compassHeading;
    uint32_t endCalibrationAt;

    /// Holds state for debug information
    DebugInfo debugInfo;

    /// Display device
    OLEDDisplay *dispdev;

    /// UI helper for rendering to frames and switching between them
    OLEDDisplayUi *ui;
};

} // namespace graphics

// Extern declarations for function symbols used in UIRenderer
extern std::vector<std::string> functionSymbol;
extern std::string functionSymbolString;
extern graphics::Screen *screen;

#endif