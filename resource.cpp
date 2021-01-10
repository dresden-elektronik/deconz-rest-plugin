/*
 * Copyright (c) 2017-2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>

#include "deconz.h"
#include "resource.h"

const char *RSensors = "/sensors";
const char *RLights = "/lights";
const char *RGroups = "/groups";
const char *RConfig = "/config";

const char *REventAdded = "event/added";
const char *REventDeleted = "event/deleted";
const char *REventValidGroup = "event/validgroup";
const char *REventCheckGroupAnyOn = "event/checkgroupanyon";

const char *RInvalidSuffix = "invalid/suffix";

const char *RAttrName = "attr/name";
const char *RAttrManufacturerName = "attr/manufacturername";
const char *RAttrModelId = "attr/modelid";
const char *RAttrType = "attr/type";
const char *RAttrClass = "attr/class";
const char *RAttrId = "attr/id";
const char *RAttrUniqueId = "attr/uniqueid";
const char *RAttrSwVersion = "attr/swversion";
const char *RAttrLastAnnounced = "attr/lastannounced";
const char *RAttrLastSeen = "attr/lastseen";

const char *RActionScene = "action/scene";

const char *RStateAirQuality = "state/airquality";
const char *RStateAirQualityPpb = "state/airqualityppb";
const char *RStateAlarm = "state/alarm";
const char *RStateAlert = "state/alert";
const char *RStateAllOn = "state/all_on";
const char *RStateAngle = "state/angle";
const char *RStateAnyOn = "state/any_on";
const char *RStateBattery = "state/battery";
const char *RStateBri = "state/bri";
const char *RStateButtonEvent = "state/buttonevent";
const char *RStateCarbonMonoxide = "state/carbonmonoxide";
const char *RStateColorMode = "state/colormode";
const char *RStateConsumption = "state/consumption";
const char *RStateCurrent = "state/current";
const char *RStateCt = "state/ct";
const char *RStateDark = "state/dark";
const char *RStateDaylight = "state/daylight";
const char *RStateEffect = "state/effect";
const char *RStateErrorCode = "state/errorcode";
const char *RStateEventDuration = "state/eventduration";
const char *RStateFire = "state/fire";
const char *RStateFlag = "state/flag";
const char *RStateFloorTemperature = "state/floortemperature";
const char *RStateGesture = "state/gesture";
const char *RStateHeating = "state/heating";
const char *RStateHue = "state/hue";
const char *RStateHumidity = "state/humidity";
const char *RStateLastCheckin = "state/lastcheckin";
const char *RStateLastSet = "state/lastset";
const char *RStateLastUpdated = "state/lastupdated";
const char *RStateLift = "state/lift";
const char *RStateLightLevel = "state/lightlevel";
const char *RStateLowBattery = "state/lowbattery";
const char *RStateLocaltime = "state/localtime";
const char *RStateLux = "state/lux";
const char *RStateMountingModeActive = "state/mountingmodeactive";
const char *RStateOn = "state/on";
const char *RStateOpen = "state/open";
const char *RStateOrientationX = "state/orientation_x";
const char *RStateOrientationY = "state/orientation_y";
const char *RStateOrientationZ = "state/orientation_z";
const char *RStatePresence = "state/presence";
const char *RStatePressure = "state/pressure";
const char *RStatePower = "state/power";
const char *RStateReachable = "state/reachable";
const char *RStateSat = "state/sat";
const char *RStateSpectralX = "state/spectral_x";
const char *RStateSpectralY = "state/spectral_y";
const char *RStateSpectralZ = "state/spectral_z";
const char *RStateSpeed = "state/speed";
const char *RStateStatus = "state/status";
const char *RStateSunrise = "state/sunrise";
const char *RStateSunset = "state/sunset";
const char *RStateUtc = "state/utc";
const char *RStateTampered = "state/tampered";
const char *RStateTemperature = "state/temperature";
const char *RStateTest = "state/test";
const char *RStateTilt = "state/tilt";
const char *RStateTiltAngle = "state/tiltangle";
const char *RStateValve = "state/valve";
const char *RStateVibration = "state/vibration";
const char *RStateVibrationStrength = "state/vibrationstrength";
const char *RStateVoltage = "state/voltage";
const char *RStateWater = "state/water";
const char *RStateWindowOpen = "state/windowopen";
const char *RStateX = "state/x";
const char *RStateY = "state/y";

const QStringList RStateEffectValues({
    "none", "colorloop"
});
const QStringList RStateEffectValuesMueller({
    "none", "colorloop", "sunset", "party", "worklight", "campfire", "romance", "nightlight"
});

const char *RConfigAlert = "config/alert";
const char *RConfigBattery = "config/battery";
const char *RConfigColorCapabilities = "config/colorcapabilities";
const char *RConfigCtMin = "config/ctmin";
const char *RConfigCtMax = "config/ctmax";
const char *RConfigConfigured = "config/configured";
const char *RConfigCoolSetpoint = "config/coolsetpoint";
const char *RConfigDelay = "config/delay";
const char *RConfigDisplayFlipped = "config/displayflipped";
const char *RConfigDuration = "config/duration";
const char *RConfigFanMode = "config/fanmode";
const char *RConfigGroup = "config/group";
const char *RConfigHeatSetpoint = "config/heatsetpoint";
const char *RConfigHostFlags = "config/hostflags";
const char *RConfigId = "config/id";
const char *RConfigLastChangeAmount = "config/lastchange_amount";
const char *RConfigLastChangeSource = "config/lastchange_source";
const char *RConfigLastChangeTime = "config/lastchange_time";
const char *RConfigLat = "config/lat";
const char *RConfigLedIndication = "config/ledindication";
const char *RConfigLocalTime = "config/localtime";
const char *RConfigLocked = "config/locked";
const char *RConfigLong = "config/long";
const char *RConfigLevelMin = "config/levelmin";
const char *RConfigMode = "config/mode";
const char *RConfigSetValve = "config/setvalve";
const char *RConfigMountingMode = "config/mountingmode";
const char *RConfigOffset = "config/offset";
const char *RConfigOn = "config/on";
const char *RConfigPending = "config/pending";
const char *RConfigPowerup = "config/powerup";
const char *RConfigPowerOnCt = "config/poweronct";
const char *RConfigPowerOnLevel = "config/poweronlevel";
const char *RConfigPreset = "config/preset";
const char *RConfigReachable = "config/reachable";
const char *RConfigSchedule = "config/schedule";
const char *RConfigScheduleOn = "config/schedule_on";
const char *RConfigSensitivity = "config/sensitivity";
const char *RConfigSensitivityMax = "config/sensitivitymax";
const char *RConfigSunriseOffset = "config/sunriseoffset";
const char *RConfigSunsetOffset = "config/sunsetoffset";
const char *RConfigSwingMode = "config/swingmode";
const char *RConfigTemperature = "config/temperature";
const char *RConfigTemperatureMeasurement = "config/temperaturemeasurement";
const char *RConfigTholdDark = "config/tholddark";
const char *RConfigTholdOffset = "config/tholdoffset";
const char *RConfigUrl = "config/url";
const char *RConfigUsertest = "config/usertest";
const char *RConfigWindowCoveringType = "config/windowcoveringtype";
const char *RConfigWindowOpen = "config/windowopen_set";
const char *RConfigUbisysJ1Mode = "config/ubisys_j1_mode";
const char *RConfigUbisysJ1WindowCoveringType = "config/ubisys_j1_windowcoveringtype";
const char *RConfigUbisysJ1ConfigurationAndStatus = "config/ubisys_j1_configurationandstatus";
const char *RConfigUbisysJ1InstalledOpenLimitLift = "config/ubisys_j1_installedopenlimitlift";
const char *RConfigUbisysJ1InstalledClosedLimitLift = "config/ubisys_j1_installedclosedlimitlift";
const char *RConfigUbisysJ1InstalledOpenLimitTilt = "config/ubisys_j1_installedopenlimittilt";
const char *RConfigUbisysJ1InstalledClosedLimitTilt = "config/ubisys_j1_installedclosedlimittilt";
const char *RConfigUbisysJ1TurnaroundGuardTime = "config/ubisys_j1_turnaroundguardtime";
const char *RConfigUbisysJ1LiftToTiltTransitionSteps = "config/ubisys_j1_lifttotilttransitionsteps";
const char *RConfigUbisysJ1TotalSteps = "config/ubisys_j1_totalsteps";
const char *RConfigUbisysJ1LiftToTiltTransitionSteps2 = "config/ubisys_j1_lifttotilttransitionsteps2";
const char *RConfigUbisysJ1TotalSteps2 = "config/ubisys_j1_totalsteps2";
const char *RConfigUbisysJ1AdditionalSteps = "config/ubisys_j1_additionalsteps";
const char *RConfigUbisysJ1InactivePowerThreshold = "config/ubisys_j1_inactivepowerthreshold";
const char *RConfigUbisysJ1StartupSteps = "config/ubisys_j1_startupsteps";

const QStringList RConfigLastChangeSourceValues({
    "manual", "schedule", "zigbee"
});

static std::vector<const char*> rPrefixes;
static std::vector<ResourceItemDescriptor> rItemDescriptors;
static const QString rInvalidString; // is returned when string is asked but not available
const ResourceItemDescriptor rInvalidItemDescriptor(DataTypeUnknown, RInvalidSuffix);

void initResourceDescriptors()
{
    rPrefixes.clear();
    rItemDescriptors.clear();

    // init resource lookup
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrName));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrManufacturerName));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrModelId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrClass));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrUniqueId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrSwVersion));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RAttrLastAnnounced));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RAttrLastSeen));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateAirQuality));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateAirQualityPpb));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAlarm));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAllOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateAngle));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAnyOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateBattery, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateBri));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateButtonEvent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateCarbonMonoxide));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateColorMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, RStateConsumption));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateCurrent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateDark));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateDaylight));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateEffect));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateErrorCode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateEventDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateFire));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateFlag));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStateFloorTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateGesture));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateHeating));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateHue));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateHumidity, 0, 10000));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateLastCheckin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateLastSet));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateLastUpdated));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateLift, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateLightLevel, 0, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateLocaltime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateLowBattery));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, RStateLux));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateMountingModeActive));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStateOrientationX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStateOrientationY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStateOrientationZ));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStatePresence));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStatePressure, 0, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStatePower));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateSat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RActionScene));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateSpectralX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateSpectralY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateSpectralZ));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateSpeed, 0, 6));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateStatus));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateSunrise));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateSunset));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateTampered));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStateTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateTest));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateTilt, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateTiltAngle));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateUtc));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateValve));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateVibration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateVibrationStrength));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateVoltage));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateWater));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateWindowOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateY));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigBattery, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigColorCapabilities));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigCtMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigCtMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigConfigured));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigCoolSetpoint, 700, 3500));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigDelay));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigDisplayFlipped));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigFanMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigGroup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigHeatSetpoint, 500, 3200));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, RConfigHostFlags));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, RConfigId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigLastChangeAmount));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigLastChangeSource));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RConfigLastChangeTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigLat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigLedIndication));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RConfigLocalTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigLocked));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigSetValve));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigLong));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigLevelMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigMountingMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigOffset, INT16_MIN, INT16_MAX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigPending));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, RConfigPowerup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigPowerOnLevel));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigPowerOnCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigPreset));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigSchedule));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigScheduleOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigSensitivity));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigSensitivityMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, RConfigSunriseOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, RConfigSunsetOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigSwingMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigTemperatureMeasurement));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigTholdDark, 0, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigTholdOffset, 1, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigUrl));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigUsertest));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigWindowCoveringType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigWindowOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigUbisysJ1Mode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigUbisysJ1WindowCoveringType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigUbisysJ1ConfigurationAndStatus));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1InstalledOpenLimitLift));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1InstalledClosedLimitLift));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1InstalledOpenLimitTilt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1InstalledClosedLimitTilt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigUbisysJ1TurnaroundGuardTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1LiftToTiltTransitionSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1TotalSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1LiftToTiltTransitionSteps2));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1TotalSteps2));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigUbisysJ1AdditionalSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1InactivePowerThreshold));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigUbisysJ1StartupSteps));
}

const char *getResourcePrefix(const QString &str)
{
    Q_UNUSED(str);
    return nullptr;
}

bool getResourceItemDescriptor(const QString &str, ResourceItemDescriptor &descr)
{
    std::vector<ResourceItemDescriptor>::const_iterator i = rItemDescriptors.begin();
    std::vector<ResourceItemDescriptor>::const_iterator end = rItemDescriptors.end();

    for (; i != end; ++i)
    {
        if (str.endsWith(QLatin1String(i->suffix)))
        {
            descr = *i;
            return true;
        }
    }

    return false;
}

/*! Copy constructor. */
ResourceItem::ResourceItem(const ResourceItem &other)
{
    *this = other;
}

/*! Move constructor. */
ResourceItem::ResourceItem(ResourceItem &&other) :
    m_isPublic(other.m_isPublic),
    m_num(other.m_num),
    m_numPrev(other.m_numPrev),
    m_str(nullptr),
    m_rid(other.m_rid),
    m_lastSet(std::move(other.m_lastSet)),
    m_lastChanged(std::move(other.m_lastChanged)),
    m_rulesInvolved(std::move(other.m_rulesInvolved))
{
    if (other.m_str) // release
    {
        m_str = other.m_str;
        other.m_str = nullptr;
    }

    other.m_rid = &rInvalidItemDescriptor;
}

/*! Destructor. */
ResourceItem::~ResourceItem()
{
    if (m_str)
    {
        delete m_str;
        m_str = nullptr;
    }
}

/*! Copy assignment. */
ResourceItem &ResourceItem::operator=(const ResourceItem &other)
{
    // self assignment?
    if (this == &other)
    {
        return *this;
    }

    m_isPublic = other.m_isPublic;
    m_num = other.m_num;
    m_numPrev = other.m_numPrev;
    m_rid = other.m_rid;
    m_lastSet = other.m_lastSet;
    m_lastChanged = other.m_lastChanged;
    m_rulesInvolved = other.m_rulesInvolved;

    if (other.m_str)
    {
        if (m_str)
        {
            *m_str = *other.m_str;
        }
        else
        {
            m_str = new QString(*other.m_str);
        }
    }
    else if (m_str)
    {
        delete m_str;
        m_str = nullptr;
    }

    return *this;
}

/*! Move assignment. */
ResourceItem &ResourceItem::operator=(ResourceItem &&other)
{
    // self assignment?
    if (this == &other)
    {
        return *this;
    }

    m_isPublic = other.m_isPublic;
    m_num = other.m_num;
    m_numPrev = other.m_numPrev;
    m_rid = other.m_rid;
    m_lastSet = std::move(other.m_lastChanged);
    m_lastChanged = std::move(other.m_lastChanged);
    m_rulesInvolved = std::move(other.m_rulesInvolved);
    other.m_rid = &rInvalidItemDescriptor;

    if (m_str)
    {
        delete m_str;
        m_str = nullptr;
    }

    if (other.m_str)
    {
        m_str = other.m_str;
        other.m_str = nullptr;
    }

    return *this;
}

/*! Initial main constructor to create a valid ResourceItem. */
ResourceItem::ResourceItem(const ResourceItemDescriptor &rid) :
    m_num(0),
    m_numPrev(0),
    m_str(nullptr),
    m_rid(&rid)
{
    if (m_rid->type == DataTypeString ||
        m_rid->type == DataTypeTime ||
        m_rid->type == DataTypeTimePattern)
    {
        m_str = new QString;
    }
}

const QString &ResourceItem::toString() const
{
    if (m_rid->type == DataTypeString ||
        m_rid->type == DataTypeTimePattern)
    {
        if (m_str)
        {
            return *m_str;
        }
    }
    else if (m_rid->type == DataTypeTime)
    {
        if (m_num > 0)
        {
            QDateTime dt;

            // default: local time in sec resolution
            QString format = QLatin1String("yyyy-MM-ddTHH:mm:ss");

            if (m_rid->suffix == RStateLastUpdated || m_rid->suffix == RStateLastCheckin)
            {
                // UTC in msec resolution
                format = QLatin1String("yyyy-MM-ddTHH:mm:ss.zzz"); // TODO add Z
                dt.setOffsetFromUtc(0);
            }
            else if (m_rid->suffix == RAttrLastAnnounced || m_rid->suffix == RStateLastSet || m_rid->suffix == RStateUtc || m_rid->suffix == RConfigLastChangeTime)
            {
                // UTC in sec resolution
                format = QLatin1String("yyyy-MM-ddTHH:mm:ssZ");
                dt.setOffsetFromUtc(0);
            }
            else if (m_rid->suffix == RAttrLastSeen)
            {
                // UTC in min resolution
                format = QLatin1String("yyyy-MM-ddTHH:mmZ");
                dt.setOffsetFromUtc(0);
            }
            else if (m_rid->suffix == RStateSunrise || m_rid->suffix == RStateSunset)
            {
                // UTC in sec resulution
                format = QLatin1String("yyyy-MM-ddTHH:mm:ss"); // TODO add Z
                dt.setOffsetFromUtc(0);
            }

            dt.setMSecsSinceEpoch(m_num);
            *m_str = dt.toString(format);
            return *m_str;
        }
    }

    return rInvalidString;
}

qint64 ResourceItem::toNumber() const
{
    return m_num;
}

qint64 ResourceItem::toNumberPrevious() const
{
    return m_numPrev;
}

bool ResourceItem::toBool() const
{
    return m_num != 0;
}

bool ResourceItem::setValue(const QString &val)
{
    if (m_str)
    {
        m_lastSet = QDateTime::currentDateTime();
        if (*m_str != val)
        {
            *m_str = val;
            m_lastChanged = m_lastSet;
        }
        return true;
    }

    return false;
}

bool ResourceItem::setValue(qint64 val)
{
    if (m_rid->validMin != 0 || m_rid->validMax != 0)
    {
        // range check
        if (val < m_rid->validMin || val > m_rid->validMax)
        {
            return false;
        }
    }

    m_lastSet = QDateTime::currentDateTime();
    m_numPrev = m_num;

    if (m_num != val)
    {
        m_num = val;
        m_lastChanged = m_lastSet;
    }

    return true;
}

bool ResourceItem::setValue(const QVariant &val)
{
    if (!val.isValid())
    {
        m_lastSet = QDateTime();
        m_lastChanged = m_lastSet;
        return true;
    }

    const auto now = QDateTime::currentDateTime();

    if (m_rid->type == DataTypeString ||
        m_rid->type == DataTypeTimePattern)
    {
        // TODO validate time pattern
        if (m_str)
        {
            m_lastSet = now;
            if (*m_str != val.toString())
            {
                *m_str = val.toString();
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }
    else if (m_rid->type == DataTypeBool)
    {
        m_lastSet = now;
        m_numPrev = m_num;

        if (m_num != val.toBool())
        {
            m_num = val.toBool();
            m_lastChanged = m_lastSet;
        }
        return true;
    }
    else if (m_rid->type == DataTypeTime)
    {
        if (val.type() == QVariant::String)
        {
            QDateTime dt = QDateTime::fromString(val.toString(), QLatin1String("yyyy-MM-ddTHH:mm:ss"));

            if (dt.isValid())
            {
                m_lastSet = now;
                m_numPrev = m_num;

                if (m_num != dt.toMSecsSinceEpoch())
                {
                    m_num = dt.toMSecsSinceEpoch();
                    m_lastChanged = m_lastSet;
                }
                return true;
            }
        }
        else if (val.type() == QVariant::DateTime)
        {
            m_lastSet = now;
            m_numPrev = m_num;

            if (m_num != val.toDateTime().toMSecsSinceEpoch())
            {
                m_num = val.toDateTime().toMSecsSinceEpoch();
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }
    else
    {
        bool ok = false;
        const int n = val.toInt(&ok);
        if (ok)
        {
            if (m_rid->validMin == 0 && m_rid->validMax == 0)
            { /* no range check */ }
            else if (n >= m_rid->validMin && n <= m_rid->validMax)
            {   /* range check: ok*/ }
            else {
                return false;
            }

            m_lastSet = now;
            m_numPrev = m_num;

            if (m_num != n)
            {
                m_num = n;
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }

    return false;
}

const ResourceItemDescriptor &ResourceItem::descriptor() const
{
    Q_ASSERT(m_rid);
    return *m_rid;
}

const QDateTime &ResourceItem::lastSet() const
{
    return m_lastSet;
}

const QDateTime &ResourceItem::lastChanged() const
{
    return m_lastChanged;
}

void ResourceItem::setTimeStamps(const QDateTime &t)
{
    m_lastSet = t;
    m_lastChanged = t;
}

QVariant ResourceItem::toVariant() const
{
    if (!m_lastSet.isValid())
    {
        return QVariant();
    }

    if (m_rid->type == DataTypeString ||
        m_rid->type == DataTypeTimePattern)
    {
        if (m_str)
        {
            return *m_str;
        }
        return QString();
    }
    else if (m_rid->type == DataTypeBool)
    {
        return (bool)m_num;
    }
    else if (m_rid->type == DataTypeTime)
    {
        return toString();
    }
    else
    {
       return (double)m_num;
    }

    return QVariant();
}

/*! Marks the resource item as involved in a rule. */
void ResourceItem::inRule(int ruleHandle)
{
    for (int handle : m_rulesInvolved)
    {
        if (handle == ruleHandle)
        {
            return;
        }
    }

    m_rulesInvolved.push_back(ruleHandle);
}

/*! Returns the rules handles in which the resource item is involved. */
const std::vector<int> ResourceItem::rulesInvolved() const
{
    return m_rulesInvolved;
}

/*! Returns true if the item should be available in the public api. */
bool ResourceItem::isPublic() const
{
    return m_isPublic;
}

/*! Sets an item should be available in the public api. */
void ResourceItem::setIsPublic(bool isPublic)
{
    m_isPublic = isPublic;
}

/*! Initial main constructor. */
Resource::Resource(const char *prefix) :
    m_prefix(prefix)
{
    Q_ASSERT(prefix == RSensors || prefix == RLights || prefix == RGroups || prefix == RConfig);
}

/*! Copy constructor. */
Resource::Resource(const Resource &other) :
    lastStatePush(other.lastStatePush),
    lastAttrPush(other.lastAttrPush),
    m_prefix(other.m_prefix),
    m_rItems(other.m_rItems)
{
    m_prefix = other.m_prefix;
}

/*! Move constructor. */
Resource::Resource(Resource &&other) :
    lastStatePush(std::move(other.lastStatePush)),
    lastAttrPush(std::move(other.lastAttrPush)),
    m_prefix(other.m_prefix),
    m_rItems(std::move(other.m_rItems))
{
    other.m_prefix = RInvalidSuffix;
}

/*! Copy assignment. */
Resource &Resource::operator=(const Resource &other)
{
    if (this != &other)
    {
        lastStatePush = other.lastStatePush;
        lastAttrPush = other.lastAttrPush;
        m_prefix = other.m_prefix;
        m_rItems = other.m_rItems;
    }
    return *this;
}

/*! Move assignment. */
Resource &Resource::operator=(Resource &&other)
{
    if (this != &other)
    {
        lastStatePush = std::move(other.lastStatePush);
        lastAttrPush = std::move(other.lastAttrPush);
        m_prefix = other.m_prefix;
        m_rItems = std::move(other.m_rItems);
    }
    return *this;
}

const char *Resource::prefix() const
{
    Q_ASSERT(m_prefix);
    return m_prefix;
}

ResourceItem *Resource::addItem(ApiDataType type, const char *suffix)
{
    ResourceItem *it = item(suffix);
    if (!it) // prevent double insertion
    {
        std::vector<ResourceItemDescriptor>::const_iterator i = rItemDescriptors.begin();
        std::vector<ResourceItemDescriptor>::const_iterator end = rItemDescriptors.end();

        for (; i != end; ++i)
        {
            if (i->suffix == suffix && i->type == type)
            {
                m_rItems.emplace_back(*i);
                return &m_rItems.back();
            }
        }

        DBG_Assert(0);
        DBG_Printf(DBG_ERROR, "unknown datatype:suffix +  %d: %s\n", type, suffix);
    }

    return it;
}

void Resource::removeItem(const char *suffix)
{
    auto i = m_rItems.begin();
    const auto end = m_rItems.end();

    for (; i != end; ++i)
    {
        if (i->descriptor().suffix != suffix)
        {
            continue;
        }

        *i = std::move(m_rItems.back());
        m_rItems.pop_back();
        break;
    }
}

ResourceItem *Resource::item(const char *suffix)
{
    for (size_t i = 0; i < m_rItems.size(); i++)
    {
        if (m_rItems[i].descriptor().suffix == suffix)
        {
            return &m_rItems[i];
        }
    }

    return nullptr;
}

const ResourceItem *Resource::item(const char *suffix) const
{
    for (size_t i = 0; i < m_rItems.size(); i++)
    {
        if (m_rItems[i].descriptor().suffix == suffix)
        {
            return &m_rItems[i];
        }
    }

    return nullptr;
}

bool Resource::toBool(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toBool();
    }
    return false;
}

qint64 Resource::toNumber(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toNumber();
    }
    return 0;
}

const QString &Resource::toString(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toString();
    }
    return rInvalidString;
}

QVariant Resource::toVariant(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toVariant();
    }
    return QVariant();
}

int Resource::itemCount() const
{
    return m_rItems.size();
}

ResourceItem *Resource::itemForIndex(size_t idx)
{
    if (idx < m_rItems.size())
    {
        return &m_rItems[idx];
    }
    return nullptr;
}

const ResourceItem *Resource::itemForIndex(size_t idx) const
{
    if (idx < m_rItems.size())
    {
        return &m_rItems[idx];
    }
    return nullptr;
}
