#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "utils/utils.h"

/*! Handle manufacturer specific Xiaomi ZCL attribute report commands to basic cluster.
 */
void DeRestPluginPrivate::handleZclAttributeReportIndicationXiaomiSpecial(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    quint16 attrId = 0;
    quint8 dataType = 0;
    quint8 length = 0;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    while (attrId == 0)
    {
        if (stream.atEnd())
        {
            break;
        }

        quint16 a;
        stream >> a;
        stream >> dataType;

        if (dataType == deCONZ::ZclCharacterString || dataType == deCONZ::ZclOctedString)
        {
            stream >> length;
        }

        if (a == 0xff01 && dataType == deCONZ::ZclCharacterString)
        {
            attrId = a;
        }
        else if (a == 0xff02 && dataType == 0x4c /*deCONZ::ZclStruct*/)
        {
            attrId = a;
        }
        else if (a == 0x00f7 && dataType == deCONZ::ZclOctedString)
        {
            attrId = a;
        }

        if (dataType == deCONZ::ZclCharacterString && attrId != 0xff01)
        {
            DBG_Printf(DBG_INFO, "0x%016llX skip Xiaomi attribute 0x%04X\n", ind.srcAddress().ext(), attrId);
            for (; length > 0; length--) // skip
            {
                quint8 dummy;
                stream >> dummy;
            }
        }
    }

    if (stream.atEnd() || attrId == 0)
    {
        return;
    }

    quint8 structIndex = 0; // only attribute id 0xff02
    quint16 structSize = 0; // only attribute id 0xff02

    quint16 battery = 0;
    quint32 lightlevel = UINT32_MAX; // use 32-bit to mark invalid and support 0xffff value
    qint16 temperature = INT16_MIN;
    quint16 humidity = UINT16_MAX;
    qint16 pressure = INT16_MIN;
    quint8 onOff = UINT8_MAX;
    quint8 onOff2 = UINT8_MAX;
    quint8 lift = UINT8_MAX;
    quint32 power = UINT32_MAX;
    quint32 consumption = UINT32_MAX;
    quint32 current = UINT32_MAX;
    quint32 voltage = UINT32_MAX;

    DBG_Printf(DBG_INFO, "0x%016llX extract Xiaomi special attribute 0x%04X\n", ind.srcAddress().ext(), attrId);

    QString dateCode;

    while (!stream.atEnd())
    {
        qint8 s8;
        qint16 s16;
        quint8 u8;
        quint16 u16;
        qint32 s32;
        quint32 u32;
        quint64 u64;
        float f;

        quint8 tag = 0;

        if (attrId == 0xff01 || attrId == 0x00f7)
        {
            stream >> tag;
        }
        else if (attrId == 0xff02)
        {
            if (structIndex == 0)
            {
                stream >> structSize; // number of elements
            }
            structIndex++;
        }

        stream >> dataType;

        switch (dataType)
        {
        case deCONZ::ZclBoolean: stream >> u8; break;
        case deCONZ::Zcl8BitInt: stream >> s8; break;
        case deCONZ::Zcl8BitUint: stream >> u8; break;
        case deCONZ::Zcl16BitInt: stream >> s16; break;
        case deCONZ::Zcl16BitUint: stream >> u16; break;
        case deCONZ::Zcl32BitInt: stream >> s32; break;
        case deCONZ::Zcl32BitUint: stream >> u32; break;
        case deCONZ::Zcl40BitUint:
            u64 = 0;
            for (int i = 0; i < 5; i++)
            {
                u64 <<= 8;
                stream >> u8;
                u64 |= u8;
            }
            break;
        case deCONZ::Zcl48BitUint:
            u64 = 0;
            for (int i = 0; i < 6; i++)
            {
                u64 <<= 8;
                stream >> u8;
                u64 |= u8;
            }
            break;
        case deCONZ::Zcl64BitUint: stream >> u64; break;
        case deCONZ::ZclSingleFloat: stream >> f; break;
        default:
        {
            DBG_Printf(DBG_INFO, "\tUnsupported datatype 0x%02X (tag 0x%02X)\n", dataType, tag);
        }
            return;
        }

        if ((tag == 0x01 || structIndex == 0x02) && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t01 battery %u (0x%04X)\n", u16, u16);
            battery = u16;
        }
        else if (tag == 0x03 && dataType == deCONZ::Zcl8BitInt)
        {
            DBG_Printf(DBG_INFO, "\t03 Device temperature %d °C\n", int(s8)); // Device temperature for lumi.plug.mmeu01
            temperature = qint16(s8) * 100;
        }
        else if ((tag == 0x04 || structIndex == 0x03) && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t04 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x05 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t05 RSSI dB (?) %u (0x%04X)\n", u16, u16); // Power outages for lumi.plug.mmeu01
        }
        else if ((tag == 0x06 || structIndex == 0x04) && dataType == deCONZ::Zcl40BitUint)
        {
            DBG_Printf(DBG_INFO, "\t06 LQI (?) %llu (0x%010llX)\n", u64, u64);
        }
        else if (tag == 0x07 && dataType == deCONZ::Zcl64BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t07 unknown %llu (0x%016llX)\n", u64, u64);
        }
        else if (tag == 0x08 && dataType == deCONZ::Zcl16BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t08 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x09 && dataType == deCONZ::Zcl16BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t09 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x0a && dataType == deCONZ::Zcl16BitUint) // lumi.vibration.aq1
        {
            DBG_Printf(DBG_INFO, "\t0a Parent NWK %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x0b && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t0b lightlevel %u (0x%04X)\n", u16, u16);
            lightlevel = u16;
        }
        else if (tag == 0x0b && dataType == deCONZ::Zcl8BitUint)
        {
            DBG_Printf(DBG_INFO, "\t0b unknown %u (0x%02X)\n", u8, u8);
        }
        else if ((tag == 0x64 || structIndex == 0x01) && dataType == deCONZ::ZclBoolean) // lumi.ctrl_ln2 endpoint 01
        {
            DBG_Printf(DBG_INFO, "\t64 on/off %u\n", u8);
            onOff = u8;
        }
        else if (tag == 0x64 && dataType == deCONZ::Zcl8BitUint) // lumi.curtain
        {
            if (u8 <= 100)
            {
                lift = 100 - u8;
            }
            DBG_Printf(DBG_INFO, "\t64 lift %u (%u%%)\n", u8, lift);
            DBG_Printf(DBG_INFO, "\t64 smoke/gas density %u (0x%02X)\n", u8, u8);   // lumi.sensor_smoke/lumi.sensor_natgas
        }
        else if (tag == 0x64 && dataType == deCONZ::Zcl16BitInt)
        {
            if (int(s16) == -10000)
            {
                DBG_Printf(DBG_INFO, "\t64 temperature %d (ignored)\n", int(s16));
            }
            else
            {
                DBG_Printf(DBG_INFO, "\t64 temperature %d\n", int(s16));
                temperature = s16;
            }
        }
        else if (tag == 0x65 && dataType == deCONZ::ZclBoolean) // lumi.ctrl_ln2 endpoint 02
        {
            DBG_Printf(DBG_INFO, "\t65 on/off %d\n", u8);
            onOff2 = u8;
        }
        else if (tag == 0x65 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t65 humidity %u\n", u16); // Mi
            humidity = u16;
        }
        else if (tag == 0x65 && dataType == deCONZ::Zcl8BitUint)
        {
            DBG_Printf(DBG_INFO, "\t65 unknown %u (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x66 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t66 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x66 && dataType == deCONZ::Zcl32BitInt) // lumi.weather
        {
            pressure = (s32 + 50) / 100;
            DBG_Printf(DBG_INFO, "\t66 pressure %d (%d)\n", s32, pressure);
        }
        else if (tag == 0x6e && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t6e unknown %u (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x6f && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t6f unknown %u (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x94 && dataType == deCONZ::Zcl8BitUint) // lumi.relay.c2acn01
        {
            DBG_Printf(DBG_INFO, "\t6f unknown %u (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x95 && dataType == deCONZ::ZclSingleFloat) // lumi.ctrl_ln2
        {
            consumption = static_cast<qint32>(round(f * 1000)); // convert to Wh
            DBG_Printf(DBG_INFO, "\t95 consumption %f (%d)\n", f, consumption);
        }
        else if (tag == 0x96 && dataType == deCONZ::ZclSingleFloat) // lumi.plug.mmeu01
        {
            voltage = static_cast<qint32>(round(f / 10)); // convert to V
            DBG_Printf(DBG_INFO, "\t96 voltage %f (%d)\n", f, voltage);
        }
        else if (tag == 0x96 && dataType == deCONZ::Zcl32BitUint) // lumi.sensor_smoke
        {
            DBG_Printf(DBG_INFO, "\t96 unknown %u (0x%08X)\n", u32, u32);
        }
        else if (tag == 0x97 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t97 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x97 && dataType == deCONZ::ZclSingleFloat) // lumi.plug.mmeu01
        {
            current = static_cast<qint32>(round(f));  // already in mA
            DBG_Printf(DBG_INFO, "\t97 current %f (%d)\n", f, current);
        }
        else if (tag == 0x98 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t98 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x98 && dataType == deCONZ::ZclSingleFloat) // lumi.ctrl_ln2
        {
            power = static_cast<qint32>(round(f));  // already in W
            DBG_Printf(DBG_INFO, "\t98 power %f (%d)\n", f, power);
        }
        else if (tag == 0x99 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t99 unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x99 && dataType == deCONZ::Zcl32BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t99 unknown %u (0x%08X)\n", u32, u32);
        }
        else if (tag == 0x9a && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t9a unknown %u (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x9a && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t9a unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x9a && dataType == deCONZ::Zcl48BitUint) // lumi.vibration.aq1
        {
            DBG_Printf(DBG_INFO, "\t9a unknown %llu (0x%012llX)\n", u64, u64);
        }
        else if (tag == 0x9b && dataType == deCONZ::Zcl16BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t9b unknown %u (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x9b && dataType == deCONZ::ZclBoolean) // lumi.plug.mmeu01
        {
            DBG_Printf(DBG_INFO, "\t9b Consumer connected (yes/no) %d\n", u8);
        }
        else if (structIndex == 0x05 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_magnet
        {
            DBG_Printf(DBG_INFO, "\tStruct index 05 unknown (counter?) %u (0x%04X)\n", u16, u16);
        }
        else if (structIndex == 0x06 && dataType == deCONZ::Zcl8BitUint) // lumi.sensor_magnet
        {
            DBG_Printf(DBG_INFO, "\tStruct index 06 unknown (counter?) %u (0x%02X)\n", u8, u8);
        }
        else if (tag)
        {
            DBG_Printf(DBG_INFO, "\t%02X unsupported tag (data type 0x%02X)\n", tag, dataType);
        }
        else if (structIndex)
        {
            DBG_Printf(DBG_INFO, "\t%02X unsupported index (data type 0x%02X)\n", structIndex, dataType);
        }
    }

    RestNodeBase *restNodePending = nullptr;
    ResourceItem *item = nullptr;
    QString modelId;

    for (LightNode &lightNode: nodes)
    {
        if (!lightNode.modelId().startsWith(QLatin1String("lumi."))) { continue; }
        if (!isSameAddress(lightNode.address(), ind.srcAddress()))   { continue; }

        quint8 stateOnOff = UINT8_MAX;
        ResourceItem *item;

        if (lightNode.modelId().startsWith(QLatin1String("lumi.ctrl_neutral")) ||
            lightNode.modelId() == QLatin1String("lumi.switch.b1lacn02") ||
            lightNode.modelId() == QLatin1String("lumi.switch.b2lacn02"))
        {
            if (lightNode.haEndpoint().endpoint() == 0x02 && onOff != UINT8_MAX)
            {
                stateOnOff = onOff;

            }
            else if (lightNode.haEndpoint().endpoint() == 0x03 && onOff2 != UINT8_MAX)
            {
                stateOnOff = onOff2;
            }
            else
            {
                continue;
            }
        }
        else if (lightNode.modelId().startsWith(QLatin1String("lumi.ctrl_ln")))
        {
            if (lightNode.haEndpoint().endpoint() == 0x01 && onOff != UINT8_MAX)
            {
                stateOnOff = onOff;
            }
            else if (lightNode.haEndpoint().endpoint() == 0x02 && onOff2 != UINT8_MAX)
            {
                stateOnOff = onOff2;
            }
            else
            {
                continue;
            }
        }
        else if (lightNode.modelId().startsWith(QLatin1String("lumi.curtain")) && lift != UINT8_MAX)
        {
            item = lightNode.item(RStateLift);
            if (item)
            {
                item->setValue(lift);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
            }
            item = lightNode.item(RStateOpen);
            bool open = lift < 100;
            if (item)
            {
                item->setValue(open);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
            }
            // FIXME: deprecate
            item = lightNode.item(RStateBri);
            if (item)
            {
                const uint bri = lift * 254 / 100;
                item->setValue(bri);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
                stateOnOff = bri != 0;
            }
            // END FIXME: deprecate
        }
        else if (onOff != UINT8_MAX)
        {
            stateOnOff = onOff;
        }

        lightNode.rx();
        item = lightNode.item(RStateReachable);
        if (item && !item->toBool())
        {
            item->setValue(true);
            enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
        }
        item = lightNode.item(RStateOn);
        if (item && stateOnOff != UINT8_MAX) // updated?
        {
            DBG_Assert(stateOnOff == 0 || stateOnOff == 1);
            
            deCONZ::NumericUnion onOffValue;
            onOffValue.u8 = stateOnOff;
            lightNode.setZclValue(NodeValue::UpdateByZclReport, ind.srcEndpoint(), ONOFF_CLUSTER_ID, 0x0000, onOffValue);
            
            item->setValue(stateOnOff);
            enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
        }
        updateLightEtag(&lightNode);
        lightNode.setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_LIGHTS;
    }

    for (Sensor &sensor : sensors)
    {
        if (sensor.deletedState() != Sensor::StateNormal || !sensor.node()) { continue; }
        if (!sensor.modelId().startsWith(QLatin1String("lumi.")))           { continue; }
        if (!isSameAddress(sensor.address(), ind.srcAddress()))             { continue; }

        if (modelId.isEmpty())
        {
            modelId = sensor.modelId();
        }

        sensor.rx();
        bool updated = false;
        restNodePending = &sensor; // remember one sensor for pending tasks

        {
            item = sensor.item(RConfigReachable);
            if (item && !item->toBool())
            {
                item->setValue(true);
                enqueueEvent(Event(RSensors, RConfigReachable, sensor.id(), item));
                updated = true;
            }
        }

        if (battery != 0)
        {
            item = sensor.item(RConfigBattery);
            // DBG_Assert(item != 0); // expected - no, lumi.ctrl_neutral2
            if (item)
            {
                // 2.7-3.0V taken from:
                // https://github.com/snalee/Xiaomi/blob/master/devicetypes/a4refillpad/xiaomi-zigbee-button.src/xiaomi-zigbee-button.groovy
                const float vmin = 2700;
                const float vmax = 3000;
                float bat = battery;

                if      (bat > vmax) { bat = vmax; }
                else if (bat < vmin) { bat = vmin; }

                bat = ((bat - vmin) /(vmax - vmin)) * 100;

                if      (bat > 100) { bat = 100; }
                else if (bat <= 0)  { bat = 1; } // ?

                item->setValue(quint8(bat));
                enqueueEvent(Event(RSensors, RConfigBattery, sensor.id(), item));
                q_ptr->nodeUpdated(sensor.address().ext(), QLatin1String(item->descriptor().suffix), QString::number(bat));

                if (item->lastSet() == item->lastChanged())
                {
                    updated = true;
                }
            }
        }

        if (temperature != INT16_MIN)
        {
            item = sensor.item(RStateTemperature);
            if (item)
            {
                ResourceItem *item2 = sensor.item(RConfigOffset);
                if (item2 && item2->toNumber() != 0)
                {
                    temperature += item2->toNumber();
                }
            }
            else
            {
                item = sensor.item(RConfigTemperature);
            }
            if (item)
            {
                item->setValue(temperature);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));

                if (item->lastSet() == item->lastChanged())
                {
                    updated = true;
                }
                if (item->descriptor().suffix == RStateTemperature)
                {
                    sensor.updateStateTimestamp();
                    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                    updated = true;
                }
            }
        }

        if (humidity != UINT16_MAX)
        {
            item = sensor.item(RStateHumidity);
            if (item)
            {
                ResourceItem *item2 = sensor.item(RConfigOffset);
                if (item2 && item2->toNumber() != 0)
                {
                    humidity += item2->toNumber();
                }
                item->setValue(humidity);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (pressure != INT16_MIN)
        {
            item = sensor.item(RStatePressure);
            if (item)
            {
                item->setValue(pressure);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (power != UINT32_MAX)
        {
            item = sensor.item(RStatePower);
            if (item)
            {
                item->setValue(power);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (consumption != UINT32_MAX)
        {
            item = sensor.item(RStateConsumption);
            if (item)
            {
                item->setValue(consumption);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (voltage != UINT32_MAX)
        {
            item = sensor.item(RStateVoltage);
            if (item)
            {
                item->setValue(voltage);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (current != UINT32_MAX)
        {
            item = sensor.item(RStateCurrent);
            if (item)
            {
                item->setValue(current);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (lightlevel != UINT32_MAX &&
            sensor.type() == QLatin1String("ZHALightLevel") &&
            sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
        {
            updateSensorLightLevel(sensor, lightlevel);
            updated = true;
        }

        if (onOff != UINT8_MAX)
        {   // don't add, just update, useful since door/window and presence sensors otherwise only report on activation
            item = sensor.item(RStateOpen);
            item = item ? item : sensor.item(RStatePresence);
            // item = item ? item : sensor.item(RStateWater);  // lumi.sensor_wleak.aq1, ignore, value is not reliable
            if (attrId == 0xff02)
            {
                // don't update Mija devices
                // e.g. lumi.sensor_motion always reports 1
            }
            else if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
            {
                // don't update Motion sensor state.
                // Imcompatibility with delay feature, and not really usefull
               sensor.updateStateTimestamp();
               enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
               updated = true;
            }
            else if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_wleak")))
            {
                // only update state timestamp assuming last known value is valid
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
            else if (item)
            {
                item->setValue(onOff);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        item = sensor.item(RAttrSwVersion);
        if (item && dateCode.isEmpty() && !item->toString().isEmpty() && !item->toString().startsWith("3000"))
        {
            dateCode = item->toString();
        }

        if (updated)
        {
            updateSensorEtag(&sensor);
            sensor.setNeedSaveDatabase(true);
            saveDatabaseItems |= DB_SENSORS;
        }
    }

    if (searchSensorsState == SearchSensorsActive)
    {
        return;
    }

    if  (!restNodePending)
    {
        return;
    }

    Resource *r = dynamic_cast<Resource*>(restNodePending);
    DBG_Assert(r != nullptr);
    
    if (!r)
    {
        return;
    }
    
    item = r->item(RAttrModelId);

    if (item && item->toString().endsWith(QLatin1String("86opcn01")))
    {
        auto *item2 = r->item(RConfigPending);
        
        if (item2 && (item2->toNumber() & R_PENDING_MODE))
        {
            // Aqara Opple switches need to be configured to send proper button events
            // send the magic word
            DBG_Printf(DBG_INFO, "Write Aqara Opple switch 0x%016llX mode attribute 0x0009 = 1\n", ind.srcAddress().ext());
            deCONZ::ZclAttribute attr(0x0009, deCONZ::Zcl8BitUint, QLatin1String("mode"), deCONZ::ZclReadWrite, false);
            attr.setValue(static_cast<quint64>(1));
            writeAttribute(restNodePending, 0x01, XIAOMI_CLUSTER_ID, attr, VENDOR_XIAOMI);
            item2->setValue(item2->toNumber() & ~R_PENDING_MODE);
        }
    }

    if (dateCode.isEmpty() && restNodePending)
    {
        // read datecode, will be applied to all sensors of this device
        readAttributes(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, { 0x0006 });
        return;
    }

    if (item && item->toString().startsWith(QLatin1String("lumi.vibration")))
    {
        ResourceItem *item2 = r->item(RConfigPending);
        ResourceItem *item3 = r->item(RConfigSensitivity);
        DBG_Assert(item2);
        DBG_Assert(item3);
        
        if (!item3->lastSet().isValid() || item2->toNumber() == 0)
        {
            if (readAttributes(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, { 0xff0d }, VENDOR_XIAOMI))
            {
                return;
            }
        }
        else
        {
            if (item2 && item2->toNumber() & R_PENDING_SENSITIVITY)
            {
                deCONZ::ZclAttribute attr(0xff0d, deCONZ::Zcl8BitUint, "sensitivity", deCONZ::ZclReadWrite, true);
                attr.setValue(static_cast<quint64>(item3->toNumber()));
                
                if (writeAttribute(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, attr, VENDOR_XIAOMI))
                {
                    item2->setValue(item2->toNumber() & ~R_PENDING_SENSITIVITY);
                    return;
                }
            }
        }
    }
}
