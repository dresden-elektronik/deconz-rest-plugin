/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"


/*! WSNDemo sensor data handler send by routers and end devices.
    \param ind a WSNDemo data frame
 */
void DeRestPluginPrivate::wsnDemoDataIndication(const deCONZ::ApsDataIndication &ind)
{
    // check valid WSNDemo endpoint
    if (ind.srcEndpoint() != 0x01)
    {
        return;
    }

    // check WSNDemo cluster
    if (ind.clusterId() != 0x0001)
    {
        return;
    }

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t msgType;
    uint8_t nodeType;
    quint64 ieeeAddr;
    uint16_t nwkAddr;
    uint32_t version;
    uint32_t channelMask;
    uint16_t panId;
    uint8_t channel;
    uint16_t parentAddr;
    uint8_t lqi;
    int8_t rssi;
    uint8_t fieldType;
    uint8_t fieldSize;
    // if fieldtype == 0x01 (sensor data)
    uint32_t battery;
    uint32_t temperature;
    uint32_t illuminance;

    stream >> msgType;
    stream >> nodeType;
    stream >> ieeeAddr;
    stream >> nwkAddr;
    stream >> version;
    stream >> channelMask;
    stream >> panId;
    stream >> channel;
    stream >> parentAddr;
    stream >> lqi;
    stream >> rssi;
    stream >> fieldType;
    stream >> fieldSize;

    if (fieldType == 0x01)
    {
        stream >> battery;
        stream >> temperature;
        stream >> illuminance;

        DBG_Printf(DBG_INFO, "Sensor 0x%016llX battery: %u, temperature: %u, light: %u\n", ieeeAddr, battery, temperature, illuminance);

      //  std::vector<Sensor>::iterator i = sensors.begin();
      //  std::vector<Sensor>::iterator end = sensors.end();
/* TODO review code for new sensor API
        for ( ; i != end; ++i)
        {
            if (i->address().ext() == ieeeAddr)
            {
                bool updated = false;

                if (i->temperature() != (double)temperature)
                {
                    updated = true;
                    i->setTemperature(temperature);
                }

                if (i->lux() != (double)illuminance)
                {
                    updated = true;
                    i->setLux(illuminance);
                }

                if (updated)
                {
                    updateEtag(i->etag);
                }

                return;
            }
        }
*/
        // does not exist yet create Sensor instance
        DBG_Printf(DBG_INFO, "found new sensor 0x%016llX\n", ieeeAddr);
        Sensor sensor;
        sensor.setName(QString("Sensor %1").arg(sensors.size() + 1));
        /*
        sensor.address().setExt(ieeeAddr);
        sensor.address().setNwk(nwkAddr);
        sensor.setId(sensor.address().toStringExt());
        sensor.setLux(illuminance);
        sensor.setTemperature(temperature);
        */
        updateEtag(sensor.etag);
        sensors.push_back(sensor);
    }

}
