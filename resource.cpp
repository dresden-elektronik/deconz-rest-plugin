/*
 * Copyright (c) 2017-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>

#include <deconz/u_assert.h>
#include <deconz/dbg_trace.h>
#include <utils/stringcache.h>
#include "resource.h"

const char *RAlarmSystems = "/alarmsystems";
const char *RConfig = "/config";
const char *RDevices = "/devices";
const char *RGroups = "/groups";
const char *RLights = "/lights";
const char *RSensors = "/sensors";

const char *REventActiveEndpoints = "event/active.endpoints";
const char *REventAdded = "event/added";
const char *REventApsConfirm = "event/aps.confirm";
const char *REventAwake = "event/awake";
const char *REventBindingTable = "event/binding.table";
const char *REventBindingTick = "event/binding.tick";
const char *REventCheckGroupAnyOn = "event/checkgroupanyon";
const char *REventDDFInitRequest = "event/ddf.init.req";
const char *REventDDFInitResponse = "event/ddf.init.rsp";
const char *REventDDFReload = "event/ddf.reload";
const char *REventDeleted = "event/deleted";
const char *REventDeviceAlarm = "event/devicealarm";
const char *REventDeviceAnnounce = "event/device.anounce";
const char *REventNodeDescriptor = "event/node.descriptor";
const char *REventPermitjoinDisabled = "event/permit.join.disabled";
const char *REventPermitjoinEnabled = "event/permit.join.enabled";
const char *REventPermitjoinRunning = "event/permit.join.running";
const char *REventPoll = "event/poll";
const char *REventSimpleDescriptor = "event/simple.descriptor";
const char *REventStartTimer = "event/start.timer";
const char *REventStateEnter = "event/state.enter";
const char *REventStateLeave = "event/state.leave";
const char *REventStateTimeout = "event/state.timeout";
const char *REventStopTimer = "event/stop.timer";
const char *REventTick = "event/tick";
const char *REventTimerFired = "event/timerfired";
const char *REventValidGroup = "event/validgroup";
const char *REventZclReadReportConfigResponse = "event/zcl.read.report.config.response";
const char *REventZclResponse = "event/zcl.response";
const char *REventZdpReload = "event/zdp.reload";
const char *REventZdpMgmtBindResponse = "event/zdp.mgmt.bind.response";
const char *REventZdpResponse = "event/zdp.response";

const char *RInvalidSuffix = "invalid/suffix";

const char *RAttrAppVersion = "attr/appversion";
const char *RAttrClass = "attr/class";
const char *RAttrConfigId = "attr/configid";
const char *RAttrDdfHash = "attr/ddf_hash";
const char *RAttrDdfPolicy = "attr/ddf_policy";
const char *RAttrExtAddress = "attr/extaddress";
const char *RAttrGroupAddress = "attr/groupaddress";
const char *RAttrId = "attr/id";
const char *RAttrLastAnnounced = "attr/lastannounced";
const char *RAttrLastSeen = "attr/lastseen";
const char *RAttrLevelMin = "attr/levelmin";
const char *RAttrManufacturerName = "attr/manufacturername";
const char *RAttrMode = "attr/mode";
const char *RAttrModelId = "attr/modelid";
const char *RAttrName = "attr/name";
const char *RAttrNwkAddress = "attr/nwkaddress";
const char *RAttrOtauFileVersion = "attr/otau/file_version";
const char *RAttrOtauImageType = "attr/otau/image_type";
const char *RAttrOtauManufacturerCode = "attr/otau/manufacturer_code";
const char *RAttrOtaVersion = "attr/otaversion";
const char *RAttrPowerOnCt = "attr/poweronct";
const char *RAttrPowerOnLevel = "attr/poweronlevel";
const char *RAttrPowerup = "attr/powerup";
const char *RAttrProductId = "attr/productid";
const char *RAttrProductName = "attr/productname";
const char *RAttrSwconfigid = "attr/swconfigid";
const char *RAttrSwVersion = "attr/swversion";
const char *RAttrSwVersionBis = "attr/swversion_bis";
const char *RAttrType = "attr/type";
const char *RAttrUniqueId = "attr/uniqueid";
const char *RAttrZoneType = "attr/zonetype";

const char *RActionScene = "action/scene";

const char *RStateAction = "state/action";
const char *RStateAirQuality = "state/airquality";
const char *RStateAirQualityBis = "state/airquality_bis";
const char *RStateAirQualityPpb = "state/airqualityppb";
const char *RStateAirQualityPpbBis = "state/airqualityppb_bis";
const char *RStateAlarm = "state/alarm";
const char *RStateAlert = "state/alert";
const char *RStateAllOn = "state/all_on";
const char *RStateAngle = "state/angle";
const char *RStateAnyOn = "state/any_on";
const char *RStateArmState = "state/armstate";
const char *RStateBattery = "state/battery";
const char *RStateBri = "state/bri";
const char *RStateButtonEvent = "state/buttonevent";
const char *RStateCarbonMonoxide = "state/carbonmonoxide";
const char *RStateCharging = "state/charging";
const char *RStateColorMode = "state/colormode";
const char *RStateConsumption = "state/consumption";
const char *RStateConsumption_2 = "state/consumption_2";
const char *RStateCt = "state/ct";
const char *RStateCurrent = "state/current";
const char *RStateCurrent_P1 = "state/current_P1";
const char *RStateCurrent_P2 = "state/current_P2";
const char *RStateCurrent_P3 = "state/current_P3";
const char *RStateDark = "state/dark";
const char *RStateDaylight = "state/daylight";
const char *RStateDeviceRunTime = "state/deviceruntime";
const char *RStateEffect = "state/effect";
const char *RStateErrorCode = "state/errorcode";
const char *RStateEventDuration = "state/eventduration";
const char *RStateExpectedEventDuration = "state/expectedeventduration";
const char *RStateExpectedRotation = "state/expectedrotation";
const char *RStateFilterRunTime = "state/filterruntime";
const char *RStateFire = "state/fire";
const char *RStateFlag = "state/flag";
const char *RStateFloorTemperature = "state/floortemperature";
const char *RStateGPDFrameCounter = "state/gpd_frame_counter";
const char *RStateGPDLastPair = "state/gpd_last_pair";
const char *RStateGesture = "state/gesture";
const char *RStateGradient = "state/gradient";
const char *RStateHeating = "state/heating";
const char *RStateHue = "state/hue";
const char *RStateHumidity = "state/humidity";
const char *RStateHumidityBis = "state/humidity_bis";
const char *RStateLastCheckin = "state/lastcheckin";
const char *RStateLastSet = "state/lastset";
const char *RStateLastUpdated = "state/lastupdated";
const char *RStateLift = "state/lift";
const char *RStateLightLevel = "state/lightlevel";
const char *RStateLocaltime = "state/localtime";
const char *RStateLockState = "state/lockstate";
const char *RStateLowBattery = "state/lowbattery";
const char *RStateLux = "state/lux";
const char *RStateMeasuredValue = "state/measured_value";
const char *RStateMoisture = "state/moisture";
const char *RStateMountingModeActive = "state/mountingmodeactive";
const char *RStateMusicSync = "state/music_sync";
const char *RStateOn = "state/on";
const char *RStateOpen = "state/open";
const char *RStateOpenBis = "state/open_bis";
const char *RStateOrientationX = "state/orientation_x";
const char *RStateOrientationY = "state/orientation_y";
const char *RStateOrientationZ = "state/orientation_z";
const char *RStatePM2_5 = "state/pm2_5";
const char *RStatePanel = "state/panel";
const char *RStatePower = "state/power";
const char *RStatePresence = "state/presence";
const char *RStatePresenceEvent = "state/presenceevent";
const char *RStatePressure = "state/pressure";
const char *RStatePressureBis = "state/pressure_bis";
const char *RStateProduction = "state/production";
const char *RStateReachable = "state/reachable";
const char *RStateReplaceFilter = "state/replacefilter";
const char *RStateRotaryEvent = "state/rotaryevent";
const char *RStateSat = "state/sat";
const char *RStateSecondsRemaining = "state/seconds_remaining";
const char *RStateSpectralX = "state/spectral_x";
const char *RStateSpectralY = "state/spectral_y";
const char *RStateSpectralZ = "state/spectral_z";
const char *RStateSpeed = "state/speed";
const char *RStateStatus = "state/status";
const char *RStateSunrise = "state/sunrise";
const char *RStateSunset = "state/sunset";
const char *RStateTampered = "state/tampered";
const char *RStateTemperature = "state/temperature";
const char *RStateTemperatureBis = "state/temperature_bis";
const char *RStateTest = "state/test";
const char *RStateTilt = "state/tilt";
const char *RStateTiltAngle = "state/tiltangle";
const char *RStateUtc = "state/utc";
const char *RStateValve = "state/valve";
const char *RStateVibration = "state/vibration";
const char *RStateVibrationStrength = "state/vibrationstrength";
const char *RStateVoltage = "state/voltage";
const char *RStateWater = "state/water";
const char *RStateWaterBis = "state/water_bis";
const char *RStateWindowOpen = "state/windowopen";
const char *RStateX = "state/x";
const char *RStateY = "state/y";

const QStringList RStateAlertValues({
    "none", "select", "lselect"
});

const QStringList RStateAlertValuesTriggerEffect({
    "none", "select", "lselect", "blink", "breathe", "okay", "channelchange", "finish", "stop"
});

const QStringList RStateEffectValues({
    "none", "colorloop"
});
const QStringList RStateEffectValuesMueller({
    "none", "colorloop", "sunset", "party", "worklight", "campfire", "romance", "nightlight"
});

const char *RCapAlertTriggerEffect = "cap/alert/trigger_effect";
const char *RCapBriMinDimLevel = "cap/bri/min_dim_level";
const char *RCapBriMoveWithOnOff = "cap/bri/move_with_onoff";
const char *RCapColorCapabilities = "cap/color/capabilities";
const char *RCapColorCtComputesXy = "cap/color/ct/computes_xy";
const char *RCapColorCtMax = "cap/color/ct/max";
const char *RCapColorCtMin = "cap/color/ct/min";
const char *RCapColorEffects = "cap/color/effects";
const char *RCapColorGamutType = "cap/color/gamut_type";
const char *RCapColorGradientMaxSegments = "cap/color/gradient/max_segments";
const char *RCapColorGradientPixelCount = "cap/color/gradient/pixel_count";
const char *RCapColorGradientPixelLength = "cap/color/gradient/pixel_length";
const char *RCapColorGradientStyles = "cap/color/gradient/styles";
const char *RCapColorXyBlueX = "cap/color/xy/blue_x";
const char *RCapColorXyBlueY = "cap/color/xy/blue_y";
const char *RCapColorXyGreenX = "cap/color/xy/green_x";
const char *RCapColorXyGreenY = "cap/color/xy/green_y";
const char *RCapColorXyRedX = "cap/color/xy/red_x";
const char *RCapColorXyRedY = "cap/color/xy/red_y";
const char *RCapGroup = "cap/group";
const char *RCapGroupsNotSupported = "cap/groups/not_supported";
const char *RCapMeasuredValueMax = "cap/measured_value/max";
const char *RCapMeasuredValueMin = "cap/measured_value/min";
const char *RCapMeasuredValueQuantity = "cap/measured_value/quantity";
const char *RCapMeasuredValueSubstance = "cap/measured_value/substance";
const char *RCapMeasuredValueUnit = "cap/measured_value/unit";
const char *RCapOnOffWithEffect = "cap/on/off_with_effect";
const char *RCapSleeper = "cap/sleeper";
const char *RCapTransitionBlock = "cap/transition_block";

const char *RConfigAlarmSystemId = "config/alarmsystemid";
const char *RConfigAlert = "config/alert";
const char *RConfigAllowTouchlink = "config/allowtouchlink";
const char *RConfigArmMode = "config/armmode";
const char *RConfigArmedAwayEntryDelay = "config/armed_away_entry_delay";
const char *RConfigArmedAwayExitDelay = "config/armed_away_exit_delay";
const char *RConfigArmedAwayTriggerDuration = "config/armed_away_trigger_duration";
const char *RConfigArmedNightEntryDelay = "config/armed_night_entry_delay";
const char *RConfigArmedNightExitDelay = "config/armed_night_exit_delay";
const char *RConfigArmedNightTriggerDuration = "config/armed_night_trigger_duration";
const char *RConfigArmedStayEntryDelay = "config/armed_stay_entry_delay";
const char *RConfigArmedStayExitDelay = "config/armed_stay_exit_delay";
const char *RConfigArmedStayTriggerDuration = "config/armed_stay_trigger_duration";
const char *RConfigBattery = "config/battery";
const char *RConfigBatteryBis = "config/battery_bis";
const char *RConfigBriCoupleCt = "config/bri/couple_ct";
const char *RConfigBriExecuteIfOff = "config/bri/execute_if_off";
const char *RConfigBriMax = "config/bri/max";
const char *RConfigBriMin = "config/bri/min";
const char *RConfigBriOnLevel = "config/bri/on_level";
const char *RConfigBriOnOffTransitiontime = "config/bri/onoff_transitiontime";
const char *RConfigBriOptions = "config/bri/options";
const char *RConfigBriStartup = "config/bri/startup";
const char *RConfigCheckin = "config/checkin";
const char *RConfigClickMode = "config/clickmode";
const char *RConfigColorCapabilities = "config/colorcapabilities";
const char *RConfigColorCtStartup = "config/color/ct/startup";
const char *RConfigColorExecuteIfOff = "config/color/execute_if_off";
const char *RConfigColorGradientPixelCount = "config/color/gradient/pixel_count";
const char *RConfigColorGradientReversed = "config/color/gradient/reversed";
const char *RConfigColorXyStartupX = "config/color/xy/startup_x";
const char *RConfigColorXyStartupY = "config/color/xy/startup_y";
const char *RConfigConfigured = "config/configured";
const char *RConfigTuyaUnlock = "config/tuya_unlock";
const char *RConfigControlSequence = "config/controlsequence";
const char *RConfigCoolSetpoint = "config/coolsetpoint";
const char *RConfigCtMax = "config/ctmax";
const char *RConfigCtMin = "config/ctmin";
const char *RConfigDelay = "config/delay";
const char *RConfigDetectionRange = "config/detectionrange";
const char *RConfigDeviceMode = "config/devicemode";
const char *RConfigDeviceModeBis = "config/devicemode_bis";
const char *RConfigDisarmedEntryDelay = "config/disarmed_entry_delay";
const char *RConfigDisarmedExitDelay = "config/disarmed_exit_delay";
const char *RConfigDisplayFlipped = "config/displayflipped";
const char *RConfigDuration = "config/duration";
const char *RConfigEnrolled = "config/enrolled";
const char *RConfigExternalTemperatureSensor = "config/externalsensortemp";
const char *RConfigExternalWindowOpen = "config/externalwindowopen";
const char *RConfigFanMode = "config/fanmode";
const char *RConfigFilterLifeTime = "config/filterlifetime";
const char *RConfigGPDDeviceId = "config/gpd_device_id";
const char *RConfigGPDKey = "config/gpd_key";
const char *RConfigGroup = "config/group";
const char *RConfigHeatSetpoint = "config/heatsetpoint";
const char *RConfigHostFlags = "config/hostflags";
const char *RConfigHumiMaxThreshold = "config/humiditymaxthreshold";
const char *RConfigHumiMinThreshold = "config/humidityminthreshold";
const char *RConfigInterfaceMode = "config/interfacemode";
const char *RConfigLastChangeAmount = "config/lastchange_amount";
const char *RConfigLastChangeSource = "config/lastchange_source";
const char *RConfigLastChangeTime = "config/lastchange_time";
const char *RConfigLat = "config/lat";
const char *RConfigLedIndication = "config/ledindication";
const char *RConfigLoadBalancing = "config/loadbalancing";
const char *RConfigLocalTime = "config/localtime";
const char *RConfigLock = "config/lock";
const char *RConfigLocked = "config/locked";
const char *RConfigLong = "config/long";
const char *RConfigMelody = "config/melody";
const char *RConfigMode = "config/mode";
const char *RConfigMountingMode = "config/mountingmode";
const char *RConfigOffset = "config/offset";
const char *RConfigOn = "config/on";
const char *RConfigOnStartup = "config/on/startup";
const char *RConfigPending = "config/pending";
const char *RConfigPreset = "config/preset";
const char *RConfigPulseConfiguration = "config/pulseconfiguration";
const char *RConfigRadiatorCovered = "config/radiatorcovered";
const char *RConfigReachable = "config/reachable";
const char *RConfigReportGrid = "config/reportgrid";
const char *RConfigResetPresence = "config/resetpresence";
const char *RConfigReversed = "config/reversed";
const char *RConfigSchedule = "config/schedule";
const char *RConfigScheduleOn = "config/schedule_on";
const char *RConfigSelfTest = "config/selftest";
const char *RConfigSensitivity = "config/sensitivity";
const char *RConfigSensitivityBis = "config/sensitivity_bis";
const char *RConfigSensitivityMax = "config/sensitivitymax";
const char *RConfigSetValve = "config/setvalve";
const char *RConfigSpeed = "config/speed";
const char *RConfigSunriseOffset = "config/sunriseoffset";
const char *RConfigSunsetOffset = "config/sunsetoffset";
const char *RConfigSwingMode = "config/swingmode";
const char *RConfigTempMaxThreshold = "config/temperaturemaxthreshold";
const char *RConfigTempMinThreshold = "config/temperatureminthreshold";
const char *RConfigTemperature = "config/temperature";
const char *RConfigTemperatureMeasurement = "config/temperaturemeasurement";
const char *RConfigTholdDark = "config/tholddark";
const char *RConfigTholdOffset = "config/tholdoffset";
const char *RConfigUnoccupiedHeatSetpoint = "config/unoccupiedheatsetpoint";
const char *RConfigTriggerDistance = "config/triggerdistance";
const char *RConfigTriggerDistanceBis = "config/triggerdistance_bis";
const char *RConfigUbisysJ1AdditionalSteps = "config/ubisys_j1_additionalsteps";
const char *RConfigUbisysJ1ConfigurationAndStatus = "config/ubisys_j1_configurationandstatus";
const char *RConfigUbisysJ1InactivePowerThreshold = "config/ubisys_j1_inactivepowerthreshold";
const char *RConfigUbisysJ1InstalledClosedLimitLift = "config/ubisys_j1_installedclosedlimitlift";
const char *RConfigUbisysJ1InstalledClosedLimitTilt = "config/ubisys_j1_installedclosedlimittilt";
const char *RConfigUbisysJ1InstalledOpenLimitLift = "config/ubisys_j1_installedopenlimitlift";
const char *RConfigUbisysJ1InstalledOpenLimitTilt = "config/ubisys_j1_installedopenlimittilt";
const char *RConfigUbisysJ1LiftToTiltTransitionSteps = "config/ubisys_j1_lifttotilttransitionsteps";
const char *RConfigUbisysJ1LiftToTiltTransitionSteps2 = "config/ubisys_j1_lifttotilttransitionsteps2";
const char *RConfigUbisysJ1Mode = "config/ubisys_j1_mode";
const char *RConfigUbisysJ1StartupSteps = "config/ubisys_j1_startupsteps";
const char *RConfigUbisysJ1TotalSteps = "config/ubisys_j1_totalsteps";
const char *RConfigUbisysJ1TotalSteps2 = "config/ubisys_j1_totalsteps2";
const char *RConfigUbisysJ1TurnaroundGuardTime = "config/ubisys_j1_turnaroundguardtime";
const char *RConfigUbisysJ1WindowCoveringType = "config/ubisys_j1_windowcoveringtype";
const char *RConfigUrl = "config/url";
const char *RConfigUsertest = "config/usertest";
const char *RConfigVolume = "config/volume";
const char *RConfigWindowCoveringType = "config/windowcoveringtype";
const char *RConfigWindowOpen = "config/windowopen_set";
const char *RConfigWindowOpenDetectionEnabled = "config/windowopendetectionenabled";

const QStringList RConfigDeviceModeValues({
    "singlerocker", "singlepushbutton", "dualrocker", "dualpushbutton"
});

const QStringList RConfigLastChangeSourceValues({
    "manual", "schedule", "zigbee"
});

static std::vector<ResourceItemDescriptor> rItemDescriptors;
static const QString rInvalidString; // is returned when string is asked but not available

R_Stats rStats;

void initResourceDescriptors()
{
    rItemDescriptors.clear();

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUnknown, QVariant::Invalid, RInvalidSuffix));

    // init resource lookup
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RAttrAppVersion));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrClass));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RAttrConfigId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrDdfHash));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrDdfPolicy));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, QVariant::Double, RAttrExtAddress));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RAttrGroupAddress));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RAttrLastAnnounced));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RAttrLastSeen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RAttrLevelMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrManufacturerName));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RAttrMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrModelId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrName));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RAttrNwkAddress));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RAttrOtauFileVersion));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RAttrOtauImageType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RAttrOtauManufacturerCode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RAttrOtaVersion));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RAttrPowerOnCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RAttrPowerOnLevel));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RAttrPowerup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrProductId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrProductName));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrSwconfigid));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrSwVersion));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrSwVersionBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RAttrUniqueId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RAttrZoneType));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RActionScene));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateAction));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateAirQuality));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateAirQualityBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateAirQualityPpb));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateAirQualityPpbBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateAlarm));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateAllOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateAngle));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateAnyOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::String, RStateArmState));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateBattery, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateBri));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, QVariant::Double, RStateButtonEvent));
    rItemDescriptors.back().flags |= ResourceItem::FlagPushOnSet;
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateCarbonMonoxide));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateCharging));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateColorMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, QVariant::Double, RStateConsumption));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, QVariant::Double, RStateConsumption_2));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateCurrent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateCurrent_P1));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateCurrent_P2));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateCurrent_P3));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateDark));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateDaylight));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RStateDeviceRunTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateEffect));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateErrorCode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RStateEventDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateExpectedEventDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateExpectedRotation));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RStateFilterRunTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateFire));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateFlag));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateFloorTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RStateGPDFrameCounter));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, QVariant::Double, RStateGPDLastPair));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, QVariant::Double, RStateGesture));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateGradient));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateHeating));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateHue));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateHumidity, 0, 10000));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateHumidityBis, 0, 10000));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateLastCheckin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateLastSet));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateLastUpdated));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateLift));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateLightLevel, 0, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateLocaltime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateLockState));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateLowBattery));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RStateLux));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeReal, QVariant::Double, RStateMeasuredValue));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateMoisture));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateMountingModeActive));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateMusicSync));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateOpenBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateOrientationX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateOrientationY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateOrientationZ));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStatePM2_5));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStatePanel));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStatePower));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStatePresence));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStatePresenceEvent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStatePressure, 0, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStatePressureBis, 0, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, QVariant::Double, RStateProduction));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateReplaceFilter));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateRotaryEvent));
    rItemDescriptors.back().flags |= ResourceItem::FlagPushOnSet;
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateSat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RStateSecondsRemaining));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateSpectralX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateSpectralY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateSpectralZ));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateSpeed, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, QVariant::Double, RStateStatus));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateSunrise));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateSunset));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateTampered));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RStateTemperatureBis, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateTest));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateTilt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateTiltAngle));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RStateUtc));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RStateValve));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateVibration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateVibrationStrength));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateVoltage));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateWater));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RStateWaterBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RStateWindowOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RStateY));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapAlertTriggerEffect));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapBriMinDimLevel));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapBriMoveWithOnOff));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorCapabilities));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapColorCtComputesXy));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorCtMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorCtMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt64, QVariant::Double, RCapColorEffects));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RCapColorGamutType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RCapColorGradientMaxSegments));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RCapColorGradientPixelCount));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorGradientPixelLength));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorGradientStyles));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorXyBlueX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorXyBlueY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorXyGreenX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorXyGreenY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorXyRedX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RCapColorXyRedY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RCapGroup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapGroupsNotSupported));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeReal, QVariant::Double, RCapMeasuredValueMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeReal, QVariant::Double, RCapMeasuredValueMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RCapMeasuredValueQuantity));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RCapMeasuredValueSubstance));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RCapMeasuredValueUnit));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapOnOffWithEffect));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapSleeper));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RCapTransitionBlock));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigAlarmSystemId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool,  QVariant::Bool, RConfigAllowTouchlink));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigArmMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedAwayEntryDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedAwayExitDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedAwayTriggerDuration, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedNightEntryDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedNightExitDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedNightTriggerDuration, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedStayEntryDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedStayExitDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigArmedStayTriggerDuration, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBattery, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBatteryBis, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool,  QVariant::Bool, RConfigBriCoupleCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool,  QVariant::Bool, RConfigBriExecuteIfOff));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBriMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBriMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBriOnLevel));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigBriOnOffTransitiontime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBriOptions));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigBriStartup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RConfigCheckin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigClickMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigColorCapabilities));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigColorCtStartup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool,  QVariant::Bool, RConfigColorExecuteIfOff));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigColorGradientPixelCount, 5, 50));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool,  QVariant::Bool, RConfigColorGradientReversed));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigColorXyStartupX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigColorXyStartupY));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigConfigured));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigTuyaUnlock));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigControlSequence));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigCoolSetpoint, 700, 3500));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigCtMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigCtMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigDelay));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RConfigDetectionRange));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigDeviceMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigDeviceModeBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigDisarmedEntryDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigDisarmedExitDelay, 0, 255));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigDisplayFlipped));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RConfigEnrolled));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigExternalTemperatureSensor));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigExternalWindowOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigFanMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RConfigFilterLifeTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigGPDDeviceId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigGPDKey));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigGroup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigHeatSetpoint, 500, 3200));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigUnoccupiedHeatSetpoint, 500, 3200));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, QVariant::Double, RConfigHostFlags));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigHumiMaxThreshold));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigHumiMinThreshold));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigInterfaceMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigLastChangeAmount));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigLastChangeSource));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RConfigLastChangeTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigLat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigLedIndication));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigLoadBalancing));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, QVariant::String, RConfigLocalTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigLock));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigLocked));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigLong));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigMelody));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigMountingMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigOffset, INT16_MIN, INT16_MAX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigOnStartup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigPending));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigPreset));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigPulseConfiguration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigRadiatorCovered));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigReportGrid));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigResetPresence));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigReversed));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigSchedule));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigScheduleOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigSelfTest));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigSensitivity));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigSensitivityBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigSensitivityMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigSpeed));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigSetValve));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigSunriseOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigSunsetOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigSwingMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigTempMaxThreshold));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, QVariant::Double, RConfigTempMinThreshold));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, QVariant::Double, RConfigTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigTemperatureMeasurement));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigTholdDark, 0, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigTholdOffset, 1, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigTriggerDistance));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigTriggerDistanceBis));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigUbisysJ1AdditionalSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigUbisysJ1ConfigurationAndStatus));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1InactivePowerThreshold));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1InstalledClosedLimitLift));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1InstalledClosedLimitTilt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1InstalledOpenLimitLift));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1InstalledOpenLimitTilt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1LiftToTiltTransitionSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1LiftToTiltTransitionSteps2));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigUbisysJ1Mode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1StartupSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1TotalSteps));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, QVariant::Double, RConfigUbisysJ1TotalSteps2));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigUbisysJ1TurnaroundGuardTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigUbisysJ1WindowCoveringType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, QVariant::String, RConfigUrl));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigUsertest));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigVolume));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, QVariant::Double, RConfigWindowCoveringType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigWindowOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, QVariant::Bool, RConfigWindowOpenDetectionEnabled));
}

const char *getResourcePrefix(const QString &str)
{
    Q_UNUSED(str);
    return nullptr;
}

bool getResourceItemDescriptor(const QString &str, ResourceItemDescriptor &descr)
{
    auto i = rItemDescriptors.begin();
    const auto end = rItemDescriptors.end();

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

bool R_AddResourceItemDescriptor(const ResourceItemDescriptor &rid)
{
    if (rid.isValid())
    {
        QLatin1String str1(rid.suffix);

        auto i = rItemDescriptors.begin();
        const auto end = rItemDescriptors.end();

        for (; i != end; ++i)
        {
            QLatin1String str2(i->suffix);

            if (str1 == str2)
            {
                return false; //already known
            }
        }

        rItemDescriptors.push_back(rid);
        return true;
    }

    return false;
}

/*! Clears \p flags in \p item which must be a numeric value item.
    The macro is used to print the flag defines as human readable.
 */
bool R_ClearFlags1(ResourceItem *item, qint64 flags, const char *strFlags)
{
    DBG_Assert(item);

    if (item)
    {
        const auto old = item->toNumber();
        if ((old & flags) != 0)
        {
            DBG_Printf(DBG_INFO_L2, "[INFO_L2] - Clear %s flags %s (0x%016llX) in 0x%016llX  --> 0x%016llX\n",
                       item->descriptor().suffix, strFlags, flags, item->toNumber(), old & ~flags);
            item->setValue(item->toNumber() & ~flags);
            return true;
        }
    }
    return false;
}

/*! Sets \p flags in \p item which must be a numeric value item.
    The macro is used to print the flag defines as human readable.
 */
bool R_SetFlags1(ResourceItem *item, qint64 flags, const char *strFlags)
{
    DBG_Assert(item);

    if (item)
    {
        const auto old = item->toNumber();
        if ((old & flags) != flags)
        {
            DBG_Printf(DBG_INFO_L2, "[INFO_L2] - Set %s flags %s (0x%016llX) in 0x%016llX --> 0x%016llX\n",
                       item->descriptor().suffix, strFlags, flags, item->toNumber(), old | flags);
            item->setValue(item->toNumber() | flags);
            return true;
        }
    }

    return false;
}

bool R_HasFlags(const ResourceItem *item, qint64 flags)
{
    DBG_Assert(item);

    if (item)
    {
        return (item->toNumber() & flags) == flags;
    }

    return false;
}

/*! Copy constructor. */
ResourceItem::ResourceItem(const ResourceItem &other)
{
    *this = other;
}

bool ResourceItem::setItemString(const QString &str)
{
    const ResourceItemDescriptor *rid = &descriptor();
    if (!(rid->type == DataTypeString ||
          rid->type == DataTypeTimePattern))
    {
        return false;
    }

    const auto utf8 = str.toUtf8();

    // for now keep all attr/* items also as atoms
    if (utf8.size() <= int(m_istr.maxSize()) && rid->suffix[0] != 'a' && rid->suffix[1] != 't')
    {
        m_istr.setString(utf8.constData());
        m_strHandle = STRING_CACHE_INVALID_HANDLE;
        return true;
    }

    m_strHandle = StringCacheAdd(utf8.constData(), (unsigned)utf8.size(), StringCacheImmutable);
    return m_strHandle != STRING_CACHE_INVALID_HANDLE;
}

/*! Move constructor. */
ResourceItem::ResourceItem(ResourceItem &&other) noexcept
{
    *this = std::move(other);
}

/*! Destructor. */
ResourceItem::~ResourceItem() noexcept
{
    if (m_str)
    {
        delete m_str;
        m_str = nullptr;
    }

    m_ridIndex = 0;
}

/*! Returns true when a value has been set but not pushed upstream. */
bool ResourceItem::needPushSet() const
{
    return (m_flags & FlagNeedPushSet) > 0;
}

/*! Returns true when a value has been set and is different from previous
    but not pushed upstream.
 */
bool ResourceItem::needPushChange() const
{
    return (m_flags & FlagNeedPushChange) > 0;
}

/*! Clears set and changed push flags, called after value has been pushed to upstream. */
void ResourceItem::clearNeedPush()
{
    m_flags &= ~static_cast<quint16>(FlagNeedPushSet | FlagNeedPushChange);
}

/*! Returns true when a value needs to be stored to database.
 */
bool ResourceItem::needStore() const
{
    return (m_flags & FlagNeedStore) > 0;
}

/*! Sets need store flag. */
void ResourceItem::setNeedStore()
{
    m_flags |= static_cast<quint16>(FlagNeedStore);
}

/*! Clears need store flag. */
void ResourceItem::clearNeedStore()
{
    m_flags &= ~static_cast<quint16>(FlagNeedStore);
}

bool ResourceItem::pushOnSet() const
{
    return (m_flags & FlagPushOnSet) > 0;
}

void ResourceItem::setPushOnSet(bool enable)
{
    if (enable)
    {
        m_flags |= static_cast<quint16>(FlagPushOnSet);
    }
    else
    {
        m_flags &= ~static_cast<quint16>(FlagPushOnSet);
    }
}

bool ResourceItem::pushOnChange() const
{
    return (m_flags & FlagPushOnChange) > 0;
}

void ResourceItem::setPushOnChange(bool enable)
{
    if (enable)
    {
        m_flags |= static_cast<quint16>(FlagPushOnChange);
    }
    else
    {
        m_flags &= ~static_cast<quint16>(FlagPushOnChange);
    }
}

bool ResourceItem::awake() const
{
    return (m_flags & FlagAwakeOnSet) > 0;
}

void ResourceItem::setAwake(bool awake)
{
    if (awake)
    {
        m_flags |= static_cast<quint16>(FlagAwakeOnSet);
    }
    else
    {
        m_flags &= ~static_cast<quint16>(FlagAwakeOnSet);
    }
}

bool ResourceItem::implicit() const
{
    return (m_flags & FlagImplicit) > 0;
}

void ResourceItem::setImplicit(bool implicit)
{
    if (implicit)
    {
        m_flags |= static_cast<quint16>(FlagImplicit);
    }
    else
    {
        m_flags &= ~static_cast<quint16>(FlagImplicit);
    }
}

void ResourceItem::setZclUnsupportedAttribute()
{
    m_flags |= static_cast<uint16_t>(FlagZclUnsupportedAttr);
}

bool ResourceItem::zclUnsupportedAttribute() const
{
    return (m_flags & FlagZclUnsupportedAttr) > 0;
}

/*! Copy assignment. */
ResourceItem &ResourceItem::operator=(const ResourceItem &other)
{
    // self assignment?
    if (this == &other)
    {
        return *this;
    }

    m_valueSource = other.m_valueSource;
    m_isPublic = other.m_isPublic;
    m_flags = other.m_flags;
    m_parseFunction = other.m_parseFunction;
    m_refreshInterval = other.m_refreshInterval;
    m_zclParam = other.m_zclParam;
    m_readEndpoint = other.m_readEndpoint;
    m_num = other.m_num;
    m_numPrev = other.m_numPrev;
    m_lastZclReport = other.m_lastZclReport;
    m_ridIndex = other.m_ridIndex;
    m_lastSet = other.m_lastSet;
    m_lastChanged = other.m_lastChanged;
    m_rulesInvolved = other.m_rulesInvolved;
    m_ddfItemHandle = other.m_ddfItemHandle;
    m_istr = other.m_istr;
    m_strHandle = other.m_strHandle;

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
ResourceItem &ResourceItem::operator=(ResourceItem &&other) noexcept
{
    // self assignment?
    if (this == &other)
    {
        return *this;
    }

    m_valueSource = other.m_valueSource;
    m_isPublic = other.m_isPublic;
    m_flags = other.m_flags;
    m_num = other.m_num;
    m_numPrev = other.m_numPrev;
    m_lastZclReport = other.m_lastZclReport;
    m_ridIndex = other.m_ridIndex;
    m_lastSet = std::move(other.m_lastSet);
    m_lastChanged = std::move(other.m_lastChanged);
    m_rulesInvolved = std::move(other.m_rulesInvolved);
    m_zclParam = other.m_zclParam;
    m_readEndpoint = other.m_readEndpoint;
    m_parseFunction = other.m_parseFunction;
    m_refreshInterval = other.m_refreshInterval;
    m_ddfItemHandle = other.m_ddfItemHandle;
    m_istr = other.m_istr;
    m_strHandle = other.m_strHandle;
    other.m_ridIndex = 0;

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
ResourceItem::ResourceItem(const ResourceItemDescriptor &rid)
{
    m_ridIndex = 0;

    for (size_t i = 0; i < rItemDescriptors.size(); i++)
    {
        if (rItemDescriptors[i].suffix == rid.suffix)
        {
            m_ridIndex = static_cast<uint16_t>(i);
            break;
        }
    }

    if (rid.type == DataTypeString ||
        rid.type == DataTypeTime ||
        rid.type == DataTypeTimePattern)
    {
        m_str = new QString;
    }

    m_flags = rid.flags;
    m_flags |= FlagPushOnChange;
}

const QString &ResourceItem::toString() const
{
    rStats.toString++;

    const ResourceItemDescriptor *rid = &descriptor();

    if (rid->type == DataTypeString ||
        rid->type == DataTypeTimePattern)
    {
        if (m_str)
        {
            return *m_str;
        }
    }
    else if (rid->type == DataTypeTime)
    {
        U_ASSERT(m_str);
        if (m_num > 0 && m_str)
        {
            QDateTime dt;

            // default: local time in sec resolution
            QString format = QLatin1String("yyyy-MM-ddTHH:mm:ss");

            if (rid->suffix == RStateLastUpdated || rid->suffix == RStateLastCheckin)
            {
                // UTC in msec resolution
                format = QLatin1String("yyyy-MM-ddTHH:mm:ss.zzz"); // TODO add Z
                dt.setOffsetFromUtc(0);
            }
            else if (rid->suffix == RAttrLastAnnounced || rid->suffix == RStateLastSet || rid->suffix == RStateUtc || rid->suffix == RConfigLastChangeTime)
            {
                // UTC in sec resolution
                format = QLatin1String("yyyy-MM-ddTHH:mm:ssZ");
                dt.setOffsetFromUtc(0);
            }
            else if (rid->suffix == RAttrLastSeen)
            {
                // UTC in min resolution
                format = QLatin1String("yyyy-MM-ddTHH:mmZ");
                dt.setOffsetFromUtc(0);
            }
            else if (rid->suffix == RStateSunrise || rid->suffix == RStateSunset)
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

QLatin1String ResourceItem::toLatin1String() const
{
    if (m_strHandle == STRING_CACHE_INVALID_HANDLE)
    {
        return m_istr;
    }
    else
    {
        const char *str;
        unsigned length;

        if (StringCacheGet(m_strHandle, &str, &length))
        {
            return QLatin1String(str, length);
        }
    }

    return QLatin1String("");
}

const char *ResourceItem::toCString() const
{
    if (m_strHandle != STRING_CACHE_INVALID_HANDLE)
    {
        const char *str;
        unsigned length;

        if (StringCacheGet(m_strHandle, &str, &length))
        {
            U_ASSERT(str[length] == '\0');
            return str;
        }
    }

    return m_istr.c_str();
}

qint64 ResourceItem::toNumber() const
{
    rStats.toNumber++;
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

/*! Sets the item string to \p str.

    \param str - utf8 string, \0 terminated
    \param length - the length of str excluding \0 (aka strlen(str))
                    can be -1 to let the length be determined automatically
 */
bool ResourceItem::setValue(const char *str, int length, ValueSource source)
{
    U_ASSERT(str);
    U_ASSERT(length >= -1);

    if (length == -1) // strlen
    {
        for (length = 0; str[length]; length++)
        {}
    }

    const QVariant val(QLatin1String(str, length));
    return setValue(val, source);
}

bool ResourceItem::setValue(const QString &val, ValueSource source)
{
    return setValue(QVariant(val), source);
}

bool ResourceItem::setValue(qint64 val, ValueSource source)
{
    const ResourceItemDescriptor *rid = &descriptor();

    if (rid->validMin != 0 || rid->validMax != 0)
    {
        // range check
        if (val < rid->validMin || val > rid->validMax)
        {
            return false;
        }
    }

    m_lastSet = QDateTime::currentDateTime();
    m_numPrev = m_num;
    m_valueSource = source;
    m_flags |= FlagNeedPushSet;
    m_flags |= FlagNeedStore;

    if (m_num != val)
    {
        m_num = val;
        m_lastChanged = m_lastSet;
        m_flags |= FlagNeedPushChange;
    }

    return true;
}

bool ResourceItem::setValue(const QVariant &val, ValueSource source)
{
    if (!val.isValid())
    {
        m_lastSet = QDateTime();
        m_lastChanged = m_lastSet;
        m_valueSource = SourceUnknown;
        return true;
    }

    const auto now = QDateTime::currentDateTime();
    m_valueSource = source;

    const ResourceItemDescriptor *rid = &descriptor();

    if (rid->type == DataTypeString ||
        rid->type == DataTypeTimePattern)
    {
        // TODO validate time pattern
        if (m_str)
        {
            m_lastSet = now;
            m_flags |= FlagNeedPushSet;
            const auto str = val.toString().trimmed();
            setItemString(str);
            if (*m_str != str)
            {
                *m_str = str;
                m_lastChanged = m_lastSet;
                m_flags |= FlagNeedPushChange;
                m_flags |= FlagNeedStore;
            }
            return true;
        }
    }
    else if (rid->type == DataTypeBool)
    {
        m_lastSet = now;
        m_numPrev = m_num;
        m_flags |= FlagNeedPushSet;
        m_flags |= FlagNeedStore;

        if (m_num != val.toBool())
        {
            m_num = val.toBool();
            m_lastChanged = m_lastSet;
            m_flags |= FlagNeedPushChange;
        }
        return true;
    }
    else if (rid->type == DataTypeTime)
    {
        if (val.type() == QVariant::String)
        {
            const auto str = val.toString();
            auto fmt = str.contains('.') ? QLatin1String("yyyy-MM-ddTHH:mm:ss.zzz")
                                         : QLatin1String("yyyy-MM-ddTHH:mm:ss");
            auto dt = QDateTime::fromString(str, fmt);
            dt.setTimeSpec(Qt::UTC);

            if (dt.isValid())
            {
                m_lastSet = now;
                m_numPrev = m_num;
                m_flags |= FlagNeedPushSet;

                if (m_num != dt.toMSecsSinceEpoch())
                {
                    m_num = dt.toMSecsSinceEpoch();
                    m_lastChanged = m_lastSet;
                    m_flags |= FlagNeedPushChange;
                    m_flags |= FlagNeedStore;
                }
                return true;
            }
        }
        else if (val.type() == QVariant::DateTime)
        {
            m_lastSet = now;
            m_numPrev = m_num;
            m_flags |= FlagNeedPushSet;

            if (m_num != val.toDateTime().toMSecsSinceEpoch())
            {
                m_num = val.toDateTime().toMSecsSinceEpoch();
                m_lastChanged = m_lastSet;
                m_flags |= FlagNeedPushChange;
                m_flags |= FlagNeedStore;
            }
            return true;
        }
    }
    else if (rid->type == DataTypeReal)
    {
        bool ok = false;
        double d = val.toDouble(&ok);

        if (ok)
        {
            m_lastSet = now;
            m_doublePrev = m_double;
            m_flags |= FlagNeedPushSet;

            if (m_double != d)
            {
                m_double = d;
                m_lastChanged = m_lastSet;
                m_flags |= FlagNeedPushChange;
                m_flags |= FlagNeedStore;
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
            if (rid->validMin == 0 && rid->validMax == 0)
            { /* no range check */ }
            else if (n >= rid->validMin && n <= rid->validMax)
            {   /* range check: ok*/ }
            else {
                m_valueSource = SourceUnknown;
                return false;
            }

            m_lastSet = now;
            m_numPrev = m_num;
            m_flags |= FlagNeedPushSet;
            m_flags |= FlagNeedStore;

            if (m_num != n)
            {
                m_num = n;
                m_lastChanged = m_lastSet;
                m_flags |= FlagNeedPushChange;
            }
            return true;
        }
    }

    m_valueSource = SourceUnknown;
    return false;
}

/*! Returns true if the items string exactls equals \p str.

    This helps to get rid of some Qt specific checks.
    So instead of

         if (item->toString() == QLatin1String("some string")) {}

    write

         if (item->equalsString("some string")) {}

    \param str - utf8 string
    \param length - optional length of str aka strlen(str)
                    the default value of -1 means the length is determined automatically
 */
bool ResourceItem::equalsString(const char *str, int length) const
{
    U_ASSERT(str);
    U_ASSERT(length >= -1);
    if (!str)
    {
        return false;
    }

    if (length == -1) // strlen
    {
        for (length = 0; str[length]; length++)
        {}
    }

    if (m_strHandle != STRING_CACHE_INVALID_HANDLE && length > 0)
    {
        const char *istr;
        unsigned ilen;
        if (StringCacheGet(m_strHandle, &istr, &ilen))
        {
            if (ilen != length)
            {
                return false;
            }

            for (int i = 0; i < length; i++)
            {
                if (str[i] != istr[i])
                {
                    return false;
                }
            }

            return true;
        }

        return false;
    }

    if (m_istr.size() == (size_t)length)
    {
        return m_istr == str;
    }

    return false;
}

const ResourceItemDescriptor &ResourceItem::descriptor() const
{
    U_ASSERT(m_ridIndex < rItemDescriptors.size());
    if (rItemDescriptors.size() <= m_ridIndex)
    {
        // invalid descriptor, note if this triggers there is likely a bug lurking somewhere
        return rItemDescriptors[0];
    }
    return rItemDescriptors[m_ridIndex];
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

    const ResourceItemDescriptor *rid = &descriptor();

    if (rid->type == DataTypeString ||
        rid->type == DataTypeTimePattern)
    {
        if (m_str)
        {
            return *m_str;
        }
        return QString();
    }
    else if (rid->type == DataTypeBool)
    {
        return (bool)m_num;
    }
    else if (rid->type == DataTypeTime)
    {
        return toString();
    }
    else if (rid->type == DataTypeReal)
    {
        return (double)m_double;
    }
    else
    {
       return (double)m_num;
    }

    return QVariant();
}

/*! Return a pointer to the submap and the key where this ResourceItemDescriptor is to be reported. */
ApiAttribute ResourceItemDescriptor::toApi(QVariantMap &attr) const
{
    QStringList keys = QString(suffix).split(QLatin1String("/"), SKIP_EMPTY_PARTS);
    DBG_Assert(keys.length() > 1);
    QString key;

    QVariantMap *p = &attr;
    bool topLevel = true;
    while (!keys.isEmpty())
    {
        key = keys.takeFirst();
        if (topLevel)
        {
            if (key == QLatin1String("attr"))
            {
                continue;
            }
            if (key == QLatin1String("cap"))
            {
                key = QLatin1String("capabilities");
            }
            topLevel = false;
        }
        if (!keys.isEmpty())
        {
            if ((*p)[key].isNull())
            {
                (*p)[key] = QVariantMap();
                p = (QVariantMap *) &((*p)[key]);
            }
            else if ((*p)[key].type() == QVariant::Map)
            {
                p = (QVariantMap *) &((*p)[key]);
            }
        }
    }
    // ApiAttribute a = ApiAttribute(p, key);
    return ApiAttribute(p, key);
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
const std::vector<int> &ResourceItem::rulesInvolved() const
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
    Q_ASSERT(prefix == RSensors || prefix == RLights || prefix == RDevices || prefix == RGroups || prefix == RConfig || prefix == RAlarmSystems);
}

/*! Copy constructor. */
Resource::Resource(const Resource &other) :
    m_handle(other.m_handle),
    m_prefix(other.m_prefix),
    m_parent(other.m_parent),
    m_rItems(other.m_rItems)
{
}

/*! Move constructor. */
Resource::Resource(Resource &&other) noexcept
{
    *this = std::move(other);
}

/*! Copy assignment. */
Resource &Resource::operator=(const Resource &other)
{
    if (this != &other)
    {
        m_handle = other.m_handle;
        m_prefix = other.m_prefix;
        m_parent = other.m_parent;
        m_rItems = other.m_rItems;
    }
    return *this;
}

/*! Move assignment. */
Resource &Resource::operator=(Resource &&other) noexcept
{
    if (this != &other)
    {
        m_handle = other.m_handle;
        m_prefix = other.m_prefix;
        m_parent = other.m_parent;
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
    rStats.item++;

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
    rStats.item++;

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

/*! Set ResourceItem value.
 * \param suffix ResourceItem suffix
 * \param val ResourceIetm value
 */
bool Resource::setValue(const char *suffix, qint64 val, bool forceUpdate)
{
    ResourceItem *i = item(suffix);
    if (!i)
    {
        return false;
    }
    if (forceUpdate || i->toNumber() != val)
    {
        if (!(i->setValue(val))) // TODO DDF ValueSource
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
bool Resource::setValue(const char *suffix, const QString &val, bool forceUpdate)
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
bool Resource::setValue(const char *suffix, const QVariant &val, bool forceUpdate)
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

/*! Adds \p stateChange to a Resource.

    If an equal StateChange already exists it will be replaced.
    TODO move out of Resource, it shouldn't depend on it.
 */
void Resource::addStateChange(const StateChange &stateChange)
{
    auto i = std::find(m_stateChanges.begin(), m_stateChanges.end(), stateChange);

    if (i != m_stateChanges.end())
    {
        *i = stateChange;
    }
    else
    {
        m_stateChanges.push_back(stateChange);
    }
}

/*! Removes all StateChange items having state StateFailed or StateFinished.

    TODO move out of Resource, it shouldn't depend on it.
 */
void Resource::cleanupStateChanges()
{
    while (!m_stateChanges.empty())
    {
        const auto i = std::find_if(m_stateChanges.begin(), m_stateChanges.end(), [](const StateChange &x)
        {
            return x.state() == StateChange::StateFailed || x.state() == StateChange::StateFinished;
        });

        if (i != m_stateChanges.end())
        {
            if (i->state() == StateChange::StateFinished)
            {
                DBG_Printf(DBG_INFO, "SC state change finished: %s\n", qPrintable(item(RAttrUniqueId)->toString()));
            }
            else if (i->state() == StateChange::StateFailed)
            {
                DBG_Printf(DBG_INFO, "SC state change failed: %s\n", qPrintable(item(RAttrUniqueId)->toString()));
            }

            m_stateChanges.erase(i);
        }
        else
        {
            break;
        }
    }
}

/*! Returns the string presentation of an data type */
QLatin1String R_DataTypeToString(ApiDataType type)
{
    switch (type)
    {
    case DataTypeUnknown: { return QLatin1String("unknown"); }
    case DataTypeBool: { return QLatin1String("bool"); }
    case DataTypeUInt8: { return QLatin1String("uint8"); }
    case DataTypeUInt16: { return QLatin1String("uint16"); }
    case DataTypeUInt32: { return QLatin1String("uint32"); }
    case DataTypeUInt64: { return QLatin1String("uint64"); }
    case DataTypeInt8: { return QLatin1String("int8"); }
    case DataTypeInt16: { return QLatin1String("int16"); }
    case DataTypeInt32: { return QLatin1String("int32"); }
    case DataTypeInt64: { return QLatin1String("int64"); }
    case DataTypeReal: { return QLatin1String("double"); }
    case DataTypeString: { return QLatin1String("string"); }
    case DataTypeTime: { return QLatin1String("ISO 8601 timestamp"); }
    case DataTypeTimePattern: { return QLatin1String("time pattern"); }
    }

    return QLatin1String("unknown");
}

/*! Returns true if \p str contains a valid list of group identifiers.

    Valid values are:
      ""          empty
      "45"        single group
      "343,123"   two groups
      "1,null,null,null"  4 groups but only first set
 */
bool isValidRConfigGroup(const QString &str)
{
    int result = 0;
    const QStringList groupList = str.split(',', SKIP_EMPTY_PARTS);

    for (const auto &groupId : groupList)
    {
        bool ok = false;
        auto gid = groupId.toUInt(&ok, 0);
        if (ok && gid <= UINT16_MAX) { result++; }
        else if (!ok && groupId == QLatin1String("null")) { result++; }
    }

    return result == groupList.size();
}
