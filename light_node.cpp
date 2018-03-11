/*
 * Copyright (c) 2016-2017 dresden elektronik ingenieurtechnik gmbh.
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
    Resource(RLights),
   m_state(StateNormal),
   m_resetRetryCount(0),
   m_zdpResetSeq(0),
   m_groupCapacity(0),
   m_manufacturer("Unknown"),
   m_manufacturerCode(0),
   m_otauClusterId(0), // unknown
   m_isOn(false),
   m_level(0),
   m_hue(0),
   m_ehue(0),
   m_normHue(0),
   m_sat(0),
   m_colorX(0),
   m_colorY(0),
   m_colorTemperature(0),
   m_colorLoopActive(false),
   m_colorLoopSpeed(0),
   m_groupCount(0),
   m_sceneCapacity(16)

{
    // add common items
    addItem(DataTypeBool, RStateOn);
    addItem(DataTypeString, RStateAlert);
    addItem(DataTypeBool, RStateReachable);
    addItem(DataTypeString, RAttrName);
    addItem(DataTypeString, RAttrModelId);
    addItem(DataTypeString, RAttrType);
}

/*! Returns the LightNode state.
 */
LightNode::State LightNode::state() const
{
    return m_state;
}

/*! Sets the LightNode state.
    \param state the LightNode state
 */
void LightNode::setState(State state)
{
    m_state = state;
}

/*! Returns true if the light is reachable.
 */
bool LightNode::isAvailable() const
{
    return item(RStateReachable)->toBool();
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
    if (m_manufacturerCode != code)
    {
        m_manufacturerCode = code;

        if (!m_manufacturer.isEmpty() && (m_manufacturer != QLatin1String("Unknown")))
        {
            return;
        }

        switch (code)
        {
        case VENDOR_ATMEL: // fall through
        case VENDOR_DDEL:    m_manufacturer = QLatin1String("dresden elektronik"); break;
        case VENDOR_BEGA:    m_manufacturer = QLatin1String("BEGA"); break;
        case VENDOR_IKEA:    m_manufacturer = QLatin1String("IKEA of Sweden"); break;
        case VENDOR_INNR:    m_manufacturer = QLatin1String("innr"); break;
        case VENDOR_INNR2:   m_manufacturer = QLatin1String("innr"); break;
        case VENDOR_INSTA:   m_manufacturer = QLatin1String("Insta"); break;
        case VENDOR_PHILIPS: m_manufacturer = QLatin1String("Philips"); break;
        case VENDOR_OSRAM_STACK: // fall through
        case VENDOR_OSRAM:   m_manufacturer = QLatin1String("OSRAM"); break;
        case VENDOR_UBISYS:  m_manufacturer = QLatin1String("ubisys"); break;
        case VENDOR_BUSCH_JAEGER:  m_manufacturer = QLatin1String("Busch-Jaeger"); break;
        case VENDOR_EMBER:   // fall through
        case VENDOR_120B:    m_manufacturer = QLatin1String("Heiman"); break;
        default:
            m_manufacturer = QLatin1String("Unknown");
            break;
        }
    }
}

/*! Returns the manufacturer name. */
const QString &LightNode::manufacturer() const
{
    return m_manufacturer;
}

/*! Sets the manufacturer name.
    \param name the manufacturer name
 */
void LightNode::setManufacturerName(const QString &name)
{
    m_manufacturer = name.trimmed();
}

/*! Returns the model indentifier.
 */
const QString &LightNode::modelId() const
{
    return item(RAttrModelId)->toString();
}

/*! Sets the model identifier.
    \param modelId the model identifier
 */
void LightNode::setModelId(const QString &modelId)
{
    item(RAttrModelId)->setValue(modelId.trimmed());
}

/*! Returns the software build identifier.
 */
const QString &LightNode::swBuildId() const
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
    return item(RAttrName)->toString();
}

/*! Sets the name of the light node.
    \param name the name
 */
void LightNode::setName(const QString &name)
{
    item(RAttrName)->setValue(name);
}

/*! Returns the device type as string for example: 'Extended color light'.
 */
const QString &LightNode::type() const
{
    return item(RAttrType)->toString();
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

/*! Returns the otau cluster id.
 */
uint16_t LightNode::otauClusterId() const
{
    return m_otauClusterId;
}

/*! Sets the otau cluster id.
    \param clusterId the cluster id
 */
void LightNode::setOtauClusterId(uint16_t clusterId)
{
    m_otauClusterId = clusterId;
}

/*! Returns true if the light supports the color cluster.
 */
bool LightNode::hasColor() const
{
    return item(RStateColorMode) != 0;
}

/*! Returns the light dimm level (0..255).
 */
uint16_t LightNode::level() const
{
    switch (m_haEndpoint.deviceId())
    {
    case DEV_ID_MAINS_POWER_OUTLET:
    case DEV_ID_HA_ONOFF_LIGHT:
//#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
        if (m_haEndpoint.profileId() == ZLL_PROFILE_ID)
        {
            // don't clash with DEV_ID_ZLL_DIMMABLE_LIGHT
            break;
        }
    case DEV_ID_ZLL_ONOFF_LIGHT:
    case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
    case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
    {
        const ResourceItem *it = item(RStateOn);
        return (it && it->toBool() ? 254 : 0);
    }

    default:
        break;
    }

    const ResourceItem *it = item(RStateBri);
    return it ? it->toNumber() : 0;
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

/*! Returns the lights mired color temperature.
    Where mired is expressed as M = 1000000 / T.
    T is the color temperature in kelvin.
 */
uint16_t LightNode::colorTemperature() const
{
    return m_colorTemperature;
}

/*! Sets the lights mired color temperature.
    \param colorTemperature the color temperature as mired value
 */
void LightNode::setColorTemperature(uint16_t colorTemperature)
{
    m_colorTemperature = colorTemperature;
}

/*! Returns the current colormode.
 */
const QString &LightNode::colorMode() const
{
    static QString foo;
    const ResourceItem *i = item(RStateColorMode);
    DBG_Assert(i != 0);
    if (i)
    {
        return i->toString();
    }
    return foo;
}

/*! Sets the current colormode.
    \param colorMode the colormode ("hs", "xy", "ct")
 */
void LightNode::setColorMode(const QString &colorMode)
{
    DBG_Assert((colorMode == "hs") || (colorMode == "xy") || (colorMode == "ct"));

    ResourceItem *i = item(RStateColorMode);
    if (i && i->toString() != colorMode)
    {
        i->setValue(colorMode);
    }
}

/*! Sets the nodes color loop active state.
    \param colorLoopActive whereever the color loop is active
 */
void LightNode::setColorLoopActive(bool colorLoopActive)
{
    m_colorLoopActive = colorLoopActive;
}

/*! Returns true if the color loop is active. */
bool LightNode::isColorLoopActive() const
{
    return m_colorLoopActive;
}

/*! Sets the nodes color loop speed state.
    \param colorLoopActive whereever the color loop is active
 */
void LightNode::setColorLoopSpeed(uint8_t speed)
{
    m_colorLoopSpeed = speed;
}

/*! Returns the nodes color loop speed state. */
uint8_t LightNode::colorLoopSpeed() const
{
    return m_colorLoopSpeed;
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
    bool isInitialized = m_haEndpoint.isValid();
    m_haEndpoint = endpoint;

    // check if std otau cluster present in endpoint
    if (otauClusterId() == 0)
    {
        QList<deCONZ::ZclCluster>::const_iterator it = endpoint.outClusters().constBegin();
        QList<deCONZ::ZclCluster>::const_iterator end = endpoint.outClusters().constEnd();

        for (; it != end; ++it)
        {
            if (it->id() == OTAU_CLUSTER_ID)
            {
                setOtauClusterId(OTAU_CLUSTER_ID);
                break;
            }
        }
    }


    // initial setup
    if (!isInitialized)
    {
        QString ltype = QLatin1String("Unknown");

        {
            QList<deCONZ::ZclCluster>::const_iterator i = endpoint.inClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator end = endpoint.inClusters().constEnd();

            for (; i != end; ++i)
            {

                if (i->id() == LEVEL_CLUSTER_ID)
                {
                    addItem(DataTypeUInt8, RStateBri);
                }
                else if (i->id() == COLOR_CLUSTER_ID)
                {
                    ResourceItem *colorMode = addItem(DataTypeString, RStateColorMode);

                    colorMode->setValue(QVariant("hs"));

                    switch (haEndpoint().deviceId())
                    {
                    case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
                    case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
                    case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
                    case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT: // fall through
                    {
                        addItem(DataTypeUInt16, RStateCt);

                        if (haEndpoint().deviceId() == DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT ||
                            haEndpoint().deviceId() == DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT)
                        {
                            colorMode->setValue(QVariant("ct"));
                        }
                    }
                    default:
                        break;
                    }

                    switch (haEndpoint().deviceId())
                    {
                    case DEV_ID_ZLL_COLOR_LIGHT:
                    case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
                    case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
                    case DEV_ID_Z30_EXTENDED_COLOR_LIGHT: // fall through
                        {
                            addItem(DataTypeUInt16, RStateX);
                            addItem(DataTypeUInt16, RStateY);
                            addItem(DataTypeUInt16, RStateHue);
                            addItem(DataTypeUInt8, RStateSat);
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        if (haEndpoint().profileId() == HA_PROFILE_ID)
        {
            switch(haEndpoint().deviceId())
            {
            //case DEV_ID_ZLL_DIMMABLE_LIGHT:   break; // clash with on/off light
            case DEV_ID_HA_ONOFF_LIGHT:
            {
                if (!item(RStateBri)) { ltype = QLatin1String("On/Off light"); }
                else                  { ltype = QLatin1String("Dimmable light"); }
            }
                break;
            case DEV_ID_ONOFF_OUTPUT:             ltype = QLatin1String("On/Off output"); break;
            case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:    ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:    ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT: ltype = QLatin1String("Dimmable plug-in unit"); break;
            case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT: ltype = QLatin1String("Dimmable plug-in unit"); break;
            case DEV_ID_HA_DIMMABLE_LIGHT:        ltype = QLatin1String("Dimmable light"); break;
            case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:  ltype = QLatin1String("Color dimmable light"); break;
            case DEV_ID_ZLL_ONOFF_LIGHT:          ltype = QLatin1String("On/Off light"); break;
            case DEV_ID_SMART_PLUG:               ltype = QLatin1String("Smart plug"); break;
            case DEV_ID_ZLL_COLOR_LIGHT:             ltype = QLatin1String("Color light"); break;
            case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:    ltype = QLatin1String("Extended color light"); break;
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:    ltype = QLatin1String("Extended color light"); break;
            case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT: ltype = QLatin1String("Color temperature light"); break;
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT: ltype = QLatin1String("Color temperature light"); break;
            case DEV_ID_XIAOMI_SMART_PLUG:           ltype = QLatin1String("Smart plug"); break;
            case DEV_ID_IAS_WARNING_DEVICE:          removeItem(RStateOn);
                                                     ltype = QLatin1String("Warning device"); break;
            default:
                break;
            }

        }
        else if (haEndpoint().profileId() == ZLL_PROFILE_ID)
        {
            switch(haEndpoint().deviceId())
            {
            case DEV_ID_ZLL_ONOFF_LIGHT:             ltype = QLatin1String("On/Off light"); break;
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:       ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:       ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:    ltype = QLatin1String("Dimmable plug-in unit"); break;
            case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:    ltype = QLatin1String("Dimmable plug-in unit"); break;
            case DEV_ID_ZLL_DIMMABLE_LIGHT:          ltype = QLatin1String("Dimmable light"); break;
            case DEV_ID_ZLL_COLOR_LIGHT:             ltype = QLatin1String("Color light"); break;
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:    ltype = QLatin1String("Extended color light"); break;
            case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:    ltype = QLatin1String("Extended color light"); break;
            case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT: ltype = QLatin1String("Color temperature light"); break;
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT: ltype = QLatin1String("Color temperature light"); break;
            default:
                break;
            }
        }

        item(RAttrType)->setValue(ltype);
    }
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

/*! Returns the resetRetryCount.
 */
uint8_t LightNode::resetRetryCount() const
{
    return m_resetRetryCount;
}

/*! Sets the resetRetryCount.
    \param resetRetryCount the resetRetryCount
 */
void LightNode::setResetRetryCount(uint8_t resetRetryCount)
{
    m_resetRetryCount = resetRetryCount;
}

/*! Returns the zdpResetSeq number.
 */
uint8_t LightNode::zdpResetSeq() const
{
    return m_zdpResetSeq;
}

/*! Sets the zdpResetSeq number.
    \param resetRetryCount the resetRetryCount
 */
void LightNode::setZdpResetSeq(uint8_t zdpResetSeq)
{
    m_zdpResetSeq = zdpResetSeq;
}

/*! Returns the group Count.
 */
uint8_t LightNode::groupCount() const
{
    return m_groupCount;
}

/*! Sets the groupCount.
    \param groupCount the groupCount
 */
void LightNode::setGroupCount(uint8_t groupCount)
{
    m_groupCount = groupCount;
}

/*! Returns the scene Capacity.
 */
uint8_t LightNode::sceneCapacity() const
{
    return m_sceneCapacity;
}

/*! Sets the scene Capacity.
    \param sceneCapacity the scene Capacity
 */
void LightNode::setSceneCapacity(uint8_t sceneCapacity)
{
    m_sceneCapacity = sceneCapacity;
}
