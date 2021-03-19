/*
 * Copyright (c) 2016-2020 dresden elektronik ingenieurtechnik gmbh.
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
    m_manufacturerCode(0),
    m_otauClusterId(0), // unknown
    m_colorLoopActive(false),
    m_colorLoopSpeed(0),
    m_groupCount(0),
    m_sceneCapacity(16)

{
    QDateTime now = QDateTime::currentDateTime();
    lastStatePush = now;
    lastAttrPush = now;

    // add common items
    addItem(DataTypeBool, RStateOn);
    addItem(DataTypeString, RStateAlert);
    addItem(DataTypeBool, RStateReachable);
    addItem(DataTypeString, RAttrName);
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeString, RAttrModelId);
    addItem(DataTypeString, RAttrType);
    addItem(DataTypeString, RAttrSwVersion);
    addItem(DataTypeString, RAttrId);
    addItem(DataTypeString, RAttrUniqueId);
    addItem(DataTypeTime, RAttrLastAnnounced);
    addItem(DataTypeTime, RAttrLastSeen);

    setManufacturerName(QLatin1String("Unknown"));
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

        if (!manufacturer().isEmpty() && (manufacturer() != QLatin1String("Unknown")))
        {
            return;
        }

        QString name;
        switch (code)
        {
        case VENDOR_ATMEL: // fall through
        case VENDOR_DDEL:    name = QLatin1String("dresden elektronik"); break;
        case VENDOR_BEGA:    name = QLatin1String("BEGA"); break;
        case VENDOR_IKEA:    name = QLatin1String("IKEA of Sweden"); break;
        case VENDOR_INNR:    name = QLatin1String("innr"); break;
        case VENDOR_LDS:     name = QLatin1String("LDS"); break;
        case VENDOR_INSTA:   name = QLatin1String("Insta"); break;
        case VENDOR_PHILIPS: name = QLatin1String("Philips"); break;
        case VENDOR_LEDVANCE: name = QLatin1String("LEDVANCE"); break;
        case VENDOR_OSRAM_STACK: // fall through
        case VENDOR_OSRAM:   name = QLatin1String("OSRAM"); break;
        case VENDOR_UBISYS:  name = QLatin1String("ubisys"); break;
        case VENDOR_BUSCH_JAEGER:  name = QLatin1String("Busch-Jaeger"); break;
        //case VENDOR_EMBER:   // fall through
        case VENDOR_HEIMAN:  name = QLatin1String("Heiman"); break;
        case VENDOR_KEEN_HOME: name = QLatin1String("Keen Home Inc"); break;
        case VENDOR_DANALOCK: name = QLatin1String("Danalock"); break;
        case VENDOR_SCHLAGE: name = QLatin1String("Schlage"); break;
        case VENDOR_DEVELCO: name = QLatin1String("Develco Products A/S"); break;
        case VENDOR_NETVOX:   name = QLatin1String("netvox"); break;
        default:
            name = QLatin1String("Unknown");
            break;
        }

        setManufacturerName(name);
    }
}

/*! Returns the manufacturer name. */
const QString &LightNode::manufacturer() const
{
    return item(RAttrManufacturerName)->toString();
}

/*! Sets the manufacturer name.
    \param name the manufacturer name
 */
void LightNode::setManufacturerName(const QString &name)
{
    item(RAttrManufacturerName)->setValue(name.trimmed());
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
    return item(RAttrSwVersion)->toString();
}

/*! Sets the software build identifier.
    \param swBuildId the software build identifier
 */
void LightNode::setSwBuildId(const QString &swBuildId)
{
    item(RAttrSwVersion)->setValue(swBuildId.trimmed());
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
    return item(RStateColorMode) != nullptr;
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

/*! Handles admin when ResourceItem value has been set.
 * \param i ResourceItem
 */
void LightNode::didSetValue(ResourceItem *i)
{
    plugin->enqueueEvent(Event(RLights, i->descriptor().suffix, id(), i));
    plugin->updateLightEtag(this);
    setNeedSaveDatabase(true);
    plugin->saveDatabaseItems |= DB_LIGHTS;
    plugin->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
}

/*! Set ResourceItem value.
 * \param suffix ResourceItem suffix
 * \param val ResourceIetm value
 */
bool LightNode::setValue(const char *suffix, qint64 val, bool forceUpdate)
{
    ResourceItem *i = item(suffix);
    if (!i)
    {
        return false;
    }
    if (forceUpdate || i->toNumber() != val)
    {
        if (!(i->setValue(val)))
        {
            return false;
        }
        didSetValue(i);
        return true;
    }
    return false;
}

/*! Set ResourceItem value.
 * \param suffix ResourceItem suffix
 * \param val ResourceIetm value
 */
bool LightNode::setValue(const char *suffix, const QString &val, bool forceUpdate)
{
    ResourceItem *i = item(suffix);
    if (!i)
    {
        return false;
    }
    if (forceUpdate || i->toString() != val)
    {
        if (!(i->setValue(val)))
        {
            return false;
        }
        didSetValue(i);
        return true;
    }
    return false;
}

/*! Set ResourceItem value.
 * \param suffix ResourceItem suffix
 * \param val ResourceIetm value
 */
bool LightNode::setValue(const char *suffix, const QVariant &val, bool forceUpdate)
{
    ResourceItem *i = item(suffix);
    if (!i)
    {
        return false;
    }
    if (forceUpdate || i->toVariant() != val)
    {
        if (!(i->setValue(val)))
        {
            return false;
        }
        didSetValue(i);
        return true;
    }
    return false;
}

/*! Mark received command and update lastseen. */
void LightNode::rx()
{
    RestNodeBase *b = static_cast<RestNodeBase *>(this);
    b->rx();
    if (lastRx() >= item(RAttrLastSeen)->lastChanged().addSecs(plugin->gwLightLastSeenInterval))
    {
        setValue(RAttrLastSeen, lastRx().toUTC());
    }
    // else
    // {
    //     item(RAttrLastSeen)->setValue(lastRx().toUTC());
    // }
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
    bool isWindowCovering = false;
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

    if (manufacturerCode() == VENDOR_XIAOMI && endpoint.deviceId() == DEV_ID_HA_COLOR_DIMMABLE_LIGHT)
    {
        // https://github.com/dresden-elektronik/deconz-rest-plugin/issues/1057
        // The Xiaomi Aqara TW (ZNLDP12LM) light is, has wrong device type in simple descriptor
        // we will treat it as color temperature light once the modelid is known
        if (modelId().isEmpty())
        {
            return; // wait until known: Xiaomi lumi.light.aqcn02
        }

        isInitialized = item(RStateColorMode) != nullptr;
    }

    // initial setup
    if (!isInitialized)
    {
        quint16 deviceId = haEndpoint().deviceId();
        QString ltype = QLatin1String("Unknown");

        {
            QList<deCONZ::ZclCluster>::const_iterator i = endpoint.inClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator end = endpoint.inClusters().constEnd();

            for (; i != end; ++i)
            {
                if (i->id() == LEVEL_CLUSTER_ID)
                {
                    if ((manufacturerCode() == VENDOR_IKEA && endpoint.deviceId() == DEV_ID_Z30_ONOFF_PLUGIN_UNIT) || // IKEA Tradfri control outlet
                        (manufacturerCode() == VENDOR_INNR && endpoint.deviceId() == DEV_ID_ZLL_ONOFF_PLUGIN_UNIT) || // innr SP120 smart plug
                        (manufacturerCode() == VENDOR_INNR && endpoint.deviceId() == DEV_ID_Z30_ONOFF_PLUGIN_UNIT) || // innr ZigBee 3.0 smart plugs (SP2xx)
                        (manufacturerCode() == VENDOR_3A_SMART_HOME && endpoint.deviceId() == DEV_ID_ZLL_ONOFF_LIGHT) || // 3A in-wall switch
                        (manufacturerCode() == VENDOR_PHILIPS && endpoint.deviceId() == DEV_ID_HA_ONOFF_LIGHT && endpoint.profileId() == HA_PROFILE_ID) ||
                        (manufacturerCode() == VENDOR_PHILIPS && endpoint.deviceId() == DEV_ID_Z30_ONOFF_PLUGIN_UNIT)) // Philips Hue plug
                    { } // skip state.bri not supported
                    else
                    {
                        addItem(DataTypeUInt8, RStateBri);
                    }
                }
                else if (i->id() == COLOR_CLUSTER_ID)
                {
                    if ((manufacturerCode() == VENDOR_NONE && deviceId == DEV_ID_ZLL_DIMMABLE_LIGHT) ||
                        (manufacturerCode() == VENDOR_NONE && deviceId == DEV_ID_LEVEL_CONTROL_SWITCH))
                    {
                        // GLEDOPTO GL-C-009 advertises non-functional color cluster
                        // ORVIBO T10D1ZW in-wall dimmer does the same
                    }
                    else
                    {
                        addItem(DataTypeString, RStateColorMode)->setValue(QVariant("hs"));
                    }

                    if (modelId() == QLatin1String("lumi.light.aqcn02"))
                    {
                        // correct wrong device id
                        deviceId = DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT;
                    }

                    switch (deviceId)
                    {
                    case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
                    case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
                    case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
                    case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT: // fall through
                    {
                        addItem(DataTypeUInt16, RConfigColorCapabilities);
                        addItem(DataTypeUInt16, RConfigCtMin);
                        addItem(DataTypeUInt16, RConfigCtMax)->setValue(0xFEFF);
                        addItem(DataTypeUInt16, RStateCt);

                        if (deviceId == DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT ||
                            deviceId == DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT)
                        {
                            item(RStateColorMode)->setValue(QVariant("ct")); // note due addItem() calls, pointer is different here
                        }
                    }
                    [[clang::fallthrough]];

                    default:
                        break;
                    }

                    switch (deviceId)
                    {
                    case DEV_ID_ZLL_COLOR_LIGHT:
                        {
                            addItem(DataTypeUInt16, RConfigColorCapabilities);
                        }
                        // fall through
                    case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
                    case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
                    case DEV_ID_Z30_EXTENDED_COLOR_LIGHT: // fall through
                        {
                            addItem(DataTypeUInt16, RStateX);
                            addItem(DataTypeUInt16, RStateY);
                            if (manufacturer() == QLatin1String("LIDL Livarno Lux"))
                            {
                                removeItem(RConfigColorCapabilities);
                            }
                            else
                            {
                                addItem(DataTypeString, RStateEffect)->setValue(RStateEffectValues[R_EFFECT_NONE]);
                                addItem(DataTypeUInt16, RStateHue);
                                addItem(DataTypeUInt8, RStateSat);
                            }
                        }
                        break;
                    default:
                        break;
                    }
                }
                else if (i->id() == WINDOW_COVERING_CLUSTER_ID /*FIXME ubisys J1*/)
                {
                    if (modelId() != QLatin1String("lumi.light.aqcn02"))
                    {
                        QList<deCONZ::ZclCluster>::const_iterator ic = haEndpoint().inClusters().constBegin();
                        std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                        std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                        isWindowCovering = true;
                        bool hasLift = true; // set default to lift
                        bool hasTilt = false;
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000)  // WindowCoveringType
                            {
                                /*
                                 * Value Type                         Capabilities
                                 *  0    Roller Shade                 = Lift only
                                 *  1    Roller Shade two motors      = Lift only
                                 *  2    Roller Shade exterior        = Lift only
                                 *  3    Roller Shade two motors ext  = Lift only
                                 *  4    Drapery                      = Lift only
                                 *  5    Awning                       = Lift only
                                 *  6    Shutter                      = Tilt only
                                 *  7    Tilt Blind Lift only         = Tilt only
                                 *  8    Tilt Blind lift & tilt       = Lift & Tilt
                                 *  9    Projector Screen             = Lift only
                                 */
                                uint8_t coveringType = ia->numericValue().u8;
                                if (coveringType == 8 ) {
                                    hasTilt = true;
                                }
                                else if (coveringType == 6 || coveringType == 7)
                                {
                                    hasTilt = true;
                                    hasLift = false;
                                }
                            }
                        }
                        removeItem(RStateAlert);
                        addItem(DataTypeBool, RStateOpen);
                        // FIXME: removeItem(RStateOn);
                        if (hasLift)
                        {
                            addItem(DataTypeUInt8, RStateLift);
                            addItem(DataTypeUInt8, RStateBri); // FIXME: deprecate
                        }
                        if (hasTilt)
                        {
                            addItem(DataTypeUInt8, RStateTilt);
                            addItem(DataTypeUInt8, RStateSat); // FIXME: deprecate
                        }
                    }
                }
                else if (i->id() == FAN_CONTROL_CLUSTER_ID) {
                    addItem(DataTypeUInt8, RStateSpeed);
                }
                //else if (i->id() == TUYA_CLUSTER_ID) {
                //}
                else if (i->id() == IAS_WD_CLUSTER_ID)
                {
                    if (modelId().startsWith(QLatin1String("902010/24")) ||   // Bitron Smoke Detector with siren
                        modelId().startsWith(QLatin1String("SMSZB-1")) ||     // Develco Smoke Alarm with siren
                        modelId().startsWith(QLatin1String("HESZB-1")) ||     // Develco heat sensor with siren
                        modelId().startsWith(QLatin1String("FLSZB-1")) ||     // Develco water leak sensor with siren
                        modelId().startsWith(QLatin1String("SIRZB-1")) ||     // Develco siren
                        modelId() == QLatin1String("902010/29") ||            // Bitron outdoor siren
                        modelId() == QLatin1String("SD8SC_00.00.03.09TC"))    // Centralite smoke sensor
                    {
                        removeItem(RStateOn);
                        ltype = QLatin1String("Warning device");
                    }
                }
                else if (i->id() == IDENTIFY_CLUSTER_ID)
                {
                    if (manufacturerCode() == VENDOR_IKEA && deviceId == DEV_ID_RANGE_EXTENDER)
                    {
                        // the repeater has no on/off cluster but an led which supports identify
                        removeItem(RStateOn);
                        ltype = QLatin1String("Range extender");
                    }
                }
            }
        }

        if (haEndpoint().profileId() == HA_PROFILE_ID)
        {

            if ((manufacturerCode() == VENDOR_LEGRAND) && isWindowCovering)
            {
                // correct wrong device id for legrand, the window suhtter command is see as plug
                // DEV_ID_Z30_ONOFF_PLUGIN_UNIT
                deviceId = DEV_ID_HA_WINDOW_COVERING_DEVICE;
            }

            switch (deviceId)
            {
            //case DEV_ID_ZLL_DIMMABLE_LIGHT:   break; // clash with on/off light
            case DEV_ID_HA_ONOFF_LIGHT:
            {
                if (item(RStateBri) == nullptr)    { ltype = QLatin1String("On/Off light"); }
                else                               { ltype = QLatin1String("Dimmable light"); }
            }
                break;
            case DEV_ID_LEVEL_CONTROL_SWITCH:          ltype = QLatin1String("Level control switch"); break;
            case DEV_ID_ONOFF_OUTPUT:                  ltype = QLatin1String("On/Off output"); break;
            case DEV_ID_LEVEL_CONTROLLABLE_OUTPUT:     ltype = QLatin1String("Level controllable output"); break;
            case DEV_ID_MAINS_POWER_OUTLET:            ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:         ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:         ltype = QLatin1String("On/Off plug-in unit"); break;
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:      ltype = QLatin1String("Dimmable plug-in unit"); break;
            case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:      ltype = QLatin1String("Dimmable plug-in unit"); break;
            case DEV_ID_HA_DIMMABLE_LIGHT:             ltype = QLatin1String("Dimmable light"); break;
            case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:       ltype = QLatin1String("Color dimmable light"); break;
            case DEV_ID_HA_ONOFF_LIGHT_SWITCH:         ltype = QLatin1String("On/Off light switch"); break;
            case DEV_ID_HA_DIMMER_SWITCH:              ltype = QLatin1String("Dimmer switch"); break;
            case DEV_ID_ZLL_ONOFF_LIGHT:               ltype = QLatin1String("On/Off light"); break;
            case DEV_ID_SMART_PLUG:                    ltype = QLatin1String("Smart plug"); break;
            case DEV_ID_ZLL_COLOR_LIGHT:               ltype = QLatin1String("Color light"); break;
            case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:      ltype = QLatin1String("Extended color light"); break;
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:      ltype = QLatin1String("Extended color light"); break;
            case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:   ltype = QLatin1String("Color temperature light"); break;
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:   ltype = QLatin1String("Color temperature light"); break;
            case DEV_ID_XIAOMI_SMART_PLUG:             ltype = QLatin1String("Smart plug"); break;
            case DEV_ID_CONSUMPTION_AWARENESS_DEVICE:  ltype = QLatin1String("Consumption awareness device");
                                                       removeItem(RStateOn); break;
            case DEV_ID_IAS_ZONE:                      removeItem(RStateOn);
                                                       ltype = QLatin1String("Warning device"); break;
            case DEV_ID_IAS_WARNING_DEVICE:            removeItem(RStateOn);
                                                       ltype = QLatin1String("Warning device"); break;
            case DEV_ID_HA_WINDOW_COVERING_CONTROLLER: ltype = QLatin1String("Window covering controller"); break;
            case DEV_ID_HA_WINDOW_COVERING_DEVICE:     ltype = QLatin1String("Window covering device"); break;
            // Danalock support. Add the device id to setHAEndPoint() to set the type to "Door lock".
            case DEV_ID_DOOR_LOCK:                     ltype = QLatin1String("Door Lock"); break;
            case DEV_ID_DOOR_LOCK_UNIT:                ltype = QLatin1String("Door Lock Unit"); break;
            
            case DEV_ID_FAN:                           ltype = QLatin1String("Fan"); break;
            case DEV_ID_CONFIGURATION_TOOL:            removeItem(RStateOn);
                                                       removeItem(RStateAlert);
                                                       ltype = QLatin1String("Configuration tool"); break;
            default:
                break;
            }
        }
        else if (haEndpoint().profileId() == ZLL_PROFILE_ID)
        {
            switch (deviceId)
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
        else if (haEndpoint().profileId() == DIN_PROFILE_ID)
        {
            switch (deviceId)
            {
            case DEV_ID_DIN_XBEE:                    removeItem(RStateOn);
                                                     removeItem(RStateAlert);
                                                     ltype = QLatin1String("Range extender"); break;
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

/*! Parse the light resource items from a JSON string. */
void LightNode::jsonToResourceItems(const QString &json)
{
    bool ok;
    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return;
    }

    QVariantMap map = var.toMap();
    QDateTime dt = QDateTime::currentDateTime().addSecs(-120);

    if (map.contains(RAttrLastAnnounced))
    {
        QString lastannounced = map[RAttrLastAnnounced].toString();
        QString format = QLatin1String("yyyy-MM-ddTHH:mm:ssZ");
        QDateTime la = QDateTime::fromString(lastannounced, format);
        la.setTimeSpec(Qt::UTC);
        map[RAttrLastAnnounced] = la;
    }

    if (map.contains(RAttrLastSeen))
    {
        QString lastseen = map[RAttrLastSeen].toString();
        QString format = lastseen.length() == 20 ? QLatin1String("yyyy-MM-ddTHH:mm:ssZ") : QLatin1String("yyyy-MM-ddTHH:mmZ");
        QDateTime ls = QDateTime::fromString(lastseen, format);
        ls.setTimeSpec(Qt::UTC);
        map[RAttrLastSeen] = ls;
        if (ls < dt)
        {
            dt = ls;
        }
    }

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const char *key = item->descriptor().suffix;

        if (map.contains(QLatin1String(key)))
        {
            item->setValue(map[key]);
            item->setTimeStamps(dt);
        }
    }
}

/*! Transfers resource items into JSON string. */
QString LightNode::resourceItemsToJson()
{
    QVariantMap map;

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const char *key = item->descriptor().suffix;
        map[key] = item->toVariant();
    }

    return Json::serialize(map);
}
