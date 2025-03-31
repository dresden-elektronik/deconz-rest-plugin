/*
 * Copyright (c) 2021-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <regex>
#include <deconz/dbg_trace.h>

#include "product_match.h"
#include "resource.h"

/*! The product map is a helper to map Basic Cluster manufacturer name and modelid
   to human readable product identifiers like marketing string or the model no. as printed on the product package.

   In case of Tuya multiple entries may refer to the same device, so in matching code
   it's best to match against the \c productId.

   Example:

   if (R_GetProductId(sensor) == QLatin1String("SEA801-ZIGBEE TRV"))
   {
   }

   Note: this will later on be replaced with the data from DDF files.
*/
struct ProductMap
{
    const char *zmanufacturerName;
    const char *zmodelId;
    const char *manufacturer;
    // a common product identifier even if multipe branded versions exist
    const char *commonProductId;
};

static const ProductMap products[] =
{
    // Prefix signification
    // --------------------
    // Tuya_THD : thermostat device using Tuya cluster
    // Tuya_COVD : covering device using Tuya cluster
    // Tuya_RPT : Repeater
    // Tuya_SEN : Sensor

    // Tuya Thermostat / TRV
    {"_TYST11_zuhszj9s", "uhszj9s", "HiHome", "Tuya_THD WZB-TRVL TRV"},
    {"_TZE200_zuhszj9s", "TS0601", "HiHome", "Tuya_THD WZB-TRVL TRV"},
    {"_TYST11_KGbxAXL2", "GbxAXL2", "Saswell", "Tuya_THD SEA801-ZIGBEE TRV"},
    {"_TYST11_c88teujp", "88teujp", "Saswell", "Tuya_THD SEA801-ZIGBEE TRV"},
    {"_TZE200_c88teujp", "TS0601", "Saswell", "Tuya_THD SEA801-ZIGBEE TRV"},
    {"_TYST11_ckud7u2l", "kud7u2l", "Tuya", "Tuya_THD HY369 TRV"},
    {"_TZE200_ckud7u2l", "TS0601", "Tuya", "Tuya_THD HY369 TRV"},
    {"_TZE200_ywdxldoj", "TS0601", "MOES/tuya", "Tuya_THD HY368 TRV"},
    {"_TZE200_fhn3negr", "TS0601", "MOES/tuya", "Tuya_THD MOES TRV"},
    {"_TZE200_aoclfnxz", "TS0601", "Moes", "Tuya_THD BTH-002 Thermostat"},
    {"_TYST11_jeaxp72v", "eaxp72v", "Essentials", "Tuya_THD Essentials TRV"},
    {"_TZE200_jeaxp72v", "TS0601", "Essentials", "Tuya_THD Essentials TRV"},
    {"_TYST11_kfvq6avy", "fvq6avy", "Revolt", "Tuya_THD NX-4911-675 TRV"},
    {"_TZE200_kfvq6avy", "TS0601", "Revolt", "Tuya_THD NX-4911-675 TRV"},
    {"_TYST11_zivfvd7h", "ivfvd7h", "Siterwell", "Tuya_THD GS361A-H04 TRV"},
    {"_TZE200_zivfvd7h", "TS0601", "Siterwell", "Tuya_THD GS361A-H04 TRV"},
    {"_TYST11_yw7cahqs", "w7cahqs", "Hama", "Tuya_THD Smart radiator TRV"},
    {"_TZE200_yw7cahqs", "TS0601", "Hama", "Tuya_THD Smart radiator TRV"},
    {"_TZE200_h4cgnbzg", "TS0601", "Hama", "Tuya_THD Smart radiator TRV"},
    {"_TZE200_cwnjrr72", "TS0601", "MOES", "Tuya_THD HY368 TRV"},
    {"_TZE200_cpmgn2cf", "TS0601", "MOES", "Tuya_THD HY368 TRV"},
    {"_TZE200_b6wax7g0", "TS0601", "MOES", "Tuya_THD BRT-100"},

    // Tuya Covering
    {"_TYST11_wmcdj3aq", "mcdj3aq", "Zemismart", "Tuya_COVD ZM25TQ"},
    {"_TZE200_wmcdj3aq", "TS0601", "Zemismart", "Tuya_COVD ZM25TQ"},
    {"_TZE200_fzo2pocs", "TS0601", "Zemismart", "Tuya_COVD ZM25TQ"},
    {"_TYST11_xu1rkty3", "u1rkty3", "Smart Home", "Tuya_COVD DT82LEMA-1.2N"},
    {"_TZE200_xuzcvlku", "TS0601", "Zemismart", "Tuya_COVD M515EGB"},
    {"_TZE200_rddyvrci", "TS0601", "Moes", "Tuya_COVD AM43-0.45/40-ES-EZ(TY)"},
    {"_TZE200_zah67ekd", "TS0601", "MoesHouse / Livolo", "Tuya_COVD AM43-0.45-40"},
    {"_TZE200_nogaemzt", "TS0601", "Tuya", "Tuya_COVD YS-MT750"},
    {"_TZE200_zpzndjez", "TS0601", "Tuya", "Tuya_COVD DS82"},
    {"_TZE200_cowvfni3", "TS0601", "Zemismart", "Tuya_COVD ZM79E-DT"},
    {"_TZE200_5zbp6j0u", "TS0601", "Tuya/Zemismart", "Tuya_COVD DT82LEMA-1.2N"},
    {"_TZE200_fdtjuw7u", "TS0601", "Yushun", "Tuya_COVD YS-MT750"},
    {"_TZE200_bqcqqjpb", "TS0601", "Yushun", "Tuya_COVD YS-MT750"},
    {"_TZE200_nueqqe6k", "TS0601", "Zemismart", "Tuya_COVD M515EGB"},
    {"_TZE200_iossyxra", "TS0601", "Zemismart", "Tuya_COVD Roller Shade"},

    // Tuya covering not using tuya cluster but need reversing
    {"_TZ3000_egq7y6pr", "TS130F", "Lonsonho", "11830304 Switch"},
    {"_TZ3000_xzqbrqk1", "TS130F", "Lonsonho", "Zigbee curtain switch"}, // https://github.com/dresden-elektronik/deconz-rest-plugin/issues/3757#issuecomment-776201454
    {"_TZ3000_ltiqubue", "TS130F", "Tuya", "Zigbee curtain switch"},
    {"_TZ3000_vd43bbfq", "TS130F", "Tuya", "QS-Zigbee-C01 Module"}, // Curtain module QS-Zigbee-C01
    {"_TZ3000_kpve0q1p", "TS130F", "Tuya", "Covering Switch ESW-2ZAD-EU"},
    {"_TZ3000_fccpjz5z", "TS130F", "Tuya", "QS-Zigbee-C01 Module"}, // Curtain module QS-Zigbee-C01
    {"_TZ3000_j1xl73iw", "TS130F", "Tuya", "Zigbee dual curtain switch"},

    // Other
    {"_TYST11_d0yu2xgi", "0yu2xgi", "NEO/Tuya", "NAS-AB02B0 Siren"},
    {"_TZE200_d0yu2xgi", "TS0601", "NEO/Tuya", "NAS-AB02B0 Siren"},
    {"_TZ3000_m0vaazab", "TS0207", "Tuya", "Tuya_RPT Repeater"},
    
    // Sensor
    {"_TZ3210_rxqls8v0", "TS0202", "Fantem", "Tuya_SEN Multi-sensor"},
    {"_TZ3210_zmy9hjay", "TS0202", "Fantem", "Tuya_SEN Multi-sensor"},
    {"_TZE200_aycxwiau", "TS0601", "Woox", "Tuya_OTH R7049 Smoke Alarm"},

     // Switch
    {"_TZE200_la2c2uo9", "TS0601", "Moes", "Tuya_DIMSWITCH MS-105Z"},
    {"_TZE200_dfxkcots", "TS0601", "Earda", "Tuya_DIMSWITCH Earda Dimmer"},
    {"_TZE200_9i9dt8is", "TS0601", "Earda", "Tuya_DIMSWITCH EDM-1ZAA-EU"},

    {nullptr, nullptr, nullptr, nullptr}
};

/*! Returns the product identifier for a matching Basic Cluster manufacturer name. */
static QLatin1String productIdForManufacturerName(const QString &manufacturerName, const ProductMap *mapIter)
{
    Q_ASSERT(mapIter);

    for (; mapIter->commonProductId != nullptr; mapIter++)
    {
        if (manufacturerName == QLatin1String(mapIter->zmanufacturerName))
        {
            return QLatin1String(mapIter->commonProductId);
        }
    }

    return QLatin1String("");
}

/*! Returns the product identifier for a resource. */
const QString R_GetProductId(Resource *resource)
{
    DBG_Assert(resource);


    if (!resource)
    {
        return { };
    }

    auto *productId = resource->item(RAttrProductId);

    if (productId)
    {
        return productId->toString();
    }

    const auto *manufacturerName = resource->item(RAttrManufacturerName);
    const auto *modelId = resource->item(RAttrModelId);

    // Need manufacturerName
    if (!manufacturerName)
    {
        return { };
    }

    //Tuya don't need modelId
    if (isTuyaManufacturerName(manufacturerName->toString()))
    {
        // for Tuya devices match against manufacturer name
        const auto productIdStr = productIdForManufacturerName(manufacturerName->toString(), products);
        if (productIdStr.size() > 0)
        {
            productId = resource->addItem(DataTypeString, RAttrProductId);
            DBG_Assert(productId);
            productId->setValue(QString(productIdStr));
            productId->setIsPublic(false); // not ready for public
            return productId->toString();
        }
        else
        {
            // Fallback
            // manufacturer name is the most unique identifier for Tuya
            if (DBG_IsEnabled(DBG_INFO_L2))
            {
                DBG_Printf(DBG_INFO_L2, "No Tuya productId entry found for manufacturername: %s\n", qPrintable(manufacturerName->toString()));
            }

            return manufacturerName->toString();
        }
    }

    if (modelId)
    {
        return modelId->toString();
    }

    return { };
}

/*! Returns true if the \p manufacturer name referes to a Tuya device. */
bool isTuyaManufacturerName(const QString &manufacturer)
{
    return manufacturer.startsWith(QLatin1String("_T")) && // quick check for performance
           std::regex_match(qPrintable(manufacturer), std::regex("_T[A-Z][A-Z0-9]{4}_[a-z0-9]{8}"));
}

// Tests for Tuya manufacturer name
/*
 Q_ASSERT(isTuyaManufacturerName("_TZ3000_bi6lpsew"));
 Q_ASSERT(isTuyaManufacturerName("_TYZB02_key8kk7r"));
 Q_ASSERT(isTuyaManufacturerName("_TYST11_ckud7u2l"));
 Q_ASSERT(isTuyaManufacturerName("_TYZB02_keyjqthh"));
 Q_ASSERT(!isTuyaManufacturerName("lumi.sensor_switch.aq2"));
*/



static const lidlDevice lidlDevices[] = { // Sorted by zigbeeManufacturerName
    { "_TYZB01_bngwdjsr", "TS1001",  "LIDL Livarno Lux", "HG06323" }, // Remote Control
    { "_TZ1800_ejwkn2h2", "TY0203",  "LIDL Silvercrest", "HG06336" }, // Contact sensor
    { "_TZ1800_fcdjzz3s", "TY0202",  "LIDL Silvercrest", "HG06335" }, // Motion sensor
    { "_TZ1800_ladpngdx", "TS0211",  "LIDL Silvercrest", "HG06668" }, // Door bell
    { "_TZ3000_1obwwnmq", "TS011F",  "LIDL Silvercrest", "HG06338" }, // Smart USB Extension Lead (EU)
    { "_TZ3000_49qchf10", "TS0502A", "LIDL Livarno Lux", "HG06492C" }, // CT Light (E27)
    { "_TZ3000_9cpuaca6", "TS0505A", "LIDL Livarno Lux", "14148906L" }, // Stimmungsleuchte
    { "_TZ3000_dbou1ap4", "TS0505A", "LIDL Livarno Lux", "HG06106C" }, // RGB Light (E27)
    { "_TZ3000_el5kt5im", "TS0502A", "LIDL Livarno Lux", "HG06492A" }, // CT Light (GU10)
    { "_TZ3000_gek6snaj", "TS0505A", "LIDL Livarno Lux", "14149506L" }, // Lichtleiste
    { "_TZ3000_kdi2o9m6", "TS011F",  "LIDL Silvercrest", "HG06337" }, // Smart plug (EU)
    { "_TZ3000_br3laukf", "TS0101",  "LIDL Silvercrest", "HG06620"}, // Garden Spike with 2 Sockets
    { "_TZ3000_kdpxju99", "TS0505A", "LIDL Livarno Lux", "HG06106A" }, // RGB Light (GU10)
    { "_TZ3000_oborybow", "TS0502A", "LIDL Livarno Lux", "HG06492B" }, // CT Light (E14)
    { "_TZ3000_odygigth", "TS0505A", "LIDL Livarno Lux", "HG06106B" }, // RGB Light (E14)
    { "_TZ3000_riwp3k79", "TS0505A", "LIDL Livarno Lux", "HG06104A" }, // LED Light Strip
    { "_TZE200_s8gkrkxk", "TS0601",  "LIDL Livarno Lux", "HG06467" }, // Smart LED String Lights (EU)
    { nullptr, nullptr, nullptr, nullptr }
};

const lidlDevice *getLidlDevice(const QString &zigbeeManufacturerName)
{
    const lidlDevice *device = lidlDevices;

    while (device->zigbeeManufacturerName != nullptr)
    {
        if (zigbeeManufacturerName == QLatin1String(device->zigbeeManufacturerName))
        {
            return device;
        }
        device++;
    }
    return nullptr;
}

bool isLidlDevice(const QString &zigbeeModelIdentifier, const QString &manufacturername)
{
    const lidlDevice *device = lidlDevices;

    while (device->zigbeeManufacturerName != nullptr)
    {
        if (zigbeeModelIdentifier == QLatin1String(device->zigbeeModelIdentifier) &&
            manufacturername == QLatin1String(device->manufacturername))
        {
            return true;
        }
        device++;
    }
    return false;
}

unsigned int productHash(const Resource *r)
{
    if (!r || !r->item(RAttrManufacturerName) || !r->item(RAttrModelId))
    {
        return 0;
    }

    if (isTuyaManufacturerName(r->item(RAttrManufacturerName)->toString()))
    {
        // for Tuya devices use manufacturer name as modelid
        return r->item(RAttrManufacturerName)->atomIndex();
    }
    else
    {
        return r->item(RAttrModelId)->atomIndex();
    }
}
