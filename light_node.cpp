/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

/*! Constructor.
 */
LightNode::LightNode() :
   m_lastRead(0),
   m_groupCapacity(0),
   m_read(0),
   m_manufacturer("Unknown"),
   m_manufacturerCode(0),
   m_isOn(false),
   m_hasColor(true),
   m_level(0),
   m_hue(0),
   m_ehue(0),
   m_normHue(0),
   m_sat(0),
   m_colorX(0),
   m_colorY(0),
   m_colorMode("hs")
{
}

/*! Returns the ZigBee Alliance manufacturer code.
 */
uint16_t LightNode::manufacturerCode() const
{
    return m_manufacturerCode;
}

/*! Sets the ZigBee Alliance manufacturer code.
    \param code the vendor specific manufacturer code
 */
void LightNode::setManufacturerCode(uint16_t code)
{
    m_manufacturerCode = code;

    switch (code)
    {
    case VENDOR_DDEL:    m_manufacturer = "dresden elektronik"; break;
    case VENDOR_PHILIPS: m_manufacturer = "Philips"; break;
    default:
        m_manufacturer = "Unknown";
        break;
    }
}

/*! Returns the manufacturer name. */
const QString &LightNode::manufacturer() const
{
    return m_manufacturer;
}

/*! Returns the model indentifier.
 */
const QString &LightNode::modelId() const
{
    return m_modelId;
}

/*! Sets the model identifier.
    \param modelId the model identifier
 */
void LightNode::setModelId(const QString &modelId)
{
    m_modelId = modelId;
}

/*! Returns the software build identifier.
 */
const QString LightNode::swBuildId() const
{
    return m_swBuildId;
}

/*! Sets the software build identifier.
    \param swBuildId the software build identifier
 */
void LightNode::setSwBuildId(const QString &swBuildId)
{
    m_swBuildId = swBuildId;
}

/*! Returns the name of the light node.
 */
const QString &LightNode::name() const
{
    return m_name;
}

/*! Sets the name of the light node.
    \param name the name
 */
void LightNode::setName(const QString &name)
{
    m_name = name;
}

/*! Returns the device type as string for example: 'Extended color light'.
 */
const QString &LightNode::type() const
{
    return m_type;
}

/*! Returns the modifiable list of groups in which the light is a member.
 */
std::vector<GroupInfo> &LightNode::groups()
{
    return m_groups;
}

/*! Returns the const list of groups in which the light is a member.
 */
const std::vector<GroupInfo> &LightNode::groups() const
{
    return m_groups;
}

/*! Returns true if the light is on.
 */
bool LightNode::isOn() const
{
    return m_isOn;
}

/*! Returns true if the light supports the color cluster.
 */
bool LightNode::hasColor() const
{
    return m_hasColor;
}

/*! Sets the on state of the light.
    \param on true if the light is on
 */
void LightNode::setIsOn(bool on)
{
    m_isOn = on;
}

/*! Returns the light dimm level (0..255).
 */
uint16_t LightNode::level() const
{
    return m_level;
}

/*! Sets the light dimm level.
    \param level the dimm level (0..255)
 */
void LightNode::setLevel(uint16_t level)
{
    DBG_Assert(level <= 255);
    if (level <= 255)
    {
        m_level = level;
    }
}

/*! Returns the lights hue (0..254).
 */
uint8_t LightNode::hue() const
{
    return m_hue;
}

/*! Sets the lights hue.
    \param hue the hue (0..254)
 */
void LightNode::setHue(uint8_t hue)
{
    DBG_Assert(hue <= 254);
    if (hue <= 254)
    {
        m_hue = hue;

        m_normHue = ((double)hue * 360.0f / 254.0f) / 360.0f;

        DBG_Assert(m_normHue >= 0.0f);
        DBG_Assert(m_normHue <= 1.0f);

        if (m_normHue < 0.0f)
        {
            m_normHue = 0.0f;
        }
        else if (m_normHue > 1.0f)
        {
            m_normHue = 1.0f;
        }

        m_ehue = (m_normHue * 65535.0f);
    }
}

/*! Returns the lights enhanced hue (0..65535).
 */
uint16_t LightNode::enhancedHue() const
{
    return m_ehue;
}

/*! Sets the lights enhanced hue.
    \param ehue the enhanced hue (0..65535)
 */
void LightNode::setEnhancedHue(uint16_t ehue)
{
    m_normHue = (((double)ehue) / 65535.0f);
    DBG_Assert(m_normHue >= 0.0f);
    DBG_Assert(m_normHue <= 1.0f);

    if (m_normHue < 0.0f)
    {
        m_normHue = 0.0f;
    }
    else if (m_normHue > 1.0f)
    {
        m_normHue = 1.0f;
    }
    m_hue = m_normHue * 254.0f;
    DBG_Assert(m_hue <= 254);

    m_ehue = ehue;
}

/*! Returns the lights saturation (0..255).
 */
uint8_t LightNode::saturation() const
{
    return m_sat;
}

/*! Sets the lights saturation.
    \param sat the saturation (0..255)
 */
void LightNode::setSaturation(uint8_t sat)
{
    m_sat = sat;
}

/*! Sets the lights CIE color coordinates.
    \param x the x coordinate (0..65279)
    \param y the y coordinate (0..65279)
 */
void LightNode::setColorXY(uint16_t x, uint16_t y)
{
    DBG_Assert(x <= 65279);
    DBG_Assert(y <= 65279);

    if (x > 65279)
    {
        x = 65279;
    }

    if (y > 65279)
    {
        y = 65279;
    }

    m_colorX = x;
    m_colorY = y;
}

/*! Returns the lights CIE X color coordinate (0.. 65279).
 */
uint16_t LightNode::colorX() const
{
    return m_colorX;
}

/*! Returns the lights CIE Y color coordinate (0.. 65279).
 */
uint16_t LightNode::colorY() const
{
    return m_colorY;
}

/*! Returns the current colormode.
 */
const QString &LightNode::colorMode() const
{
    return m_colorMode;
}

/*! Sets the current colormode.
 * \param colorMode the colormode ("hs", "xy", "ct")
 */
void LightNode::setColorMode(const QString &colorMode)
{
    DBG_Assert((colorMode == "hs") || (colorMode == "xy") || (colorMode == "ct"));
    m_colorMode = colorMode;
}

/*! Returns the lights HA endpoint descriptor.
 */
const deCONZ::SimpleDescriptor &LightNode::haEndpoint() const
{
    return m_haEndpoint;
}

/*! Sets the lights HA endpoint descriptor.
    \param endpoint the HA endpoint descriptor
 */
void LightNode::setHaEndpoint(const deCONZ::SimpleDescriptor &endpoint)
{
    m_haEndpoint = endpoint;

    // update device type string if not known already
    if (m_type.isEmpty())
    {
        if (haEndpoint().profileId() == HA_PROFILE_ID)
        {
            switch(haEndpoint().deviceId())
            {
            case DEV_ID_HA_ONOFF_LIGHT:           m_type = "On/Off light"; m_hasColor = false; break;
            case DEV_ID_HA_DIMMABLE_LIGHT:        m_type = "Dimmable light"; m_hasColor = false; break;
            case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:  m_type = "Color dimmable light"; m_hasColor = true; break;
            default:
                break;
            }

        }
        else if (haEndpoint().profileId() == ZLL_PROFILE_ID)
        {
            switch(haEndpoint().deviceId())
            {
            case DEV_ID_ZLL_ONOFF_LIGHT:             m_type = "On/Off light"; m_hasColor = false; break;
            case DEV_ID_ZLL_DIMMABLE_LIGHT:          m_type = "Dimmable light"; m_hasColor = false; break;
            case DEV_ID_ZLL_COLOR_LIGHT:             m_type = "Color light"; m_hasColor = true; break;
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:    m_type = "Extended color light"; m_hasColor = true; break;
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT: m_type = "Color temperature light"; m_hasColor = true; break;
            default:
                break;
            }
        }
    }

    if (m_type.isEmpty())
    {
        m_type = "Unknown";
    }
}

/*! Check if some data must be queried from the node.
    \param readFlags or combined bitmap of READ_* values
    \return true if every flag in readFlags is set
*/
bool LightNode::mustRead(uint32_t readFlags)
{
    if ((m_read & readFlags) == readFlags)
    {
        return true;
    }

    return false;
}

/*! Enables all flags given in \p readFlags in the read set.
    \param readFlags or combined bitmap of READ_* values
 */
void LightNode::enableRead(uint32_t readFlags)
{
    m_read |= readFlags;
}

/*! Clears all flags given in \p readFlags in the read set.
    \param readFlags or combined bitmap of READ_* values
 */
void LightNode::clearRead(uint32_t readFlags)
{
    m_read &= ~readFlags;
}

/*! Returns the group capacity.
 */
uint8_t LightNode::groupCapacity() const
{
    return m_groupCapacity;
}

/*! Sets the group capacity.
    \param capacity the group capacity
 */
void LightNode::setGroupCapacity(uint8_t capacity)
{
    m_groupCapacity = capacity;
}

/*! Returns the time than the next auto reading is queued.
 */
const QTime &LightNode::nextReadTime() const
{
    return m_nextReadTime;
}

/*! Sets the time than the next auto reading should be queued.
    \param time the time for reading
 */
void LightNode::setNextReadTime(const QTime &time)
{
    m_nextReadTime = time;
}

/*! Returns the value of the idleTotalCounter than the last reading happend.
 */
int LightNode::lastRead() const
{
    return m_lastRead;
}

/*! Sets the last read counter.
    \param lastRead copy of idleTotalCounter
 */
void LightNode::setLastRead(int lastRead)
{
    m_lastRead = lastRead;
}
