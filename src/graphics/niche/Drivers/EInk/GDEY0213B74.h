/*

E-Ink display driver
    - GDEY0213B74
    - Manufacturer: Goodisplay
    - Size: 2.13 inch
    - Resolution: 200px x 200px
    - Flex connector marking: FPC-A002

    Warning: UNTESTED

*/

#pragma once

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "configuration.h"

#include "./SSD16XX.h"

namespace NicheGraphics::Drivers
{
class GDEY0213B74 : public SSD16XX
{
    // Display properties
  private:
    static constexpr uint32_t width = 122;
    static constexpr uint32_t height = 250;
    static constexpr UpdateTypes supported = (UpdateTypes)(FULL | FAST);

  public:
    GDEY0213B74() : SSD16XX(width, height, supported) {}

  protected:
    virtual void configScanning() override;
    virtual void configWaveform() override;
    virtual void configUpdateSequence() override;
    void detachFromUpdate() override;
};

} // namespace NicheGraphics::Drivers

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS