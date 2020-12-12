/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */


#ifndef GREEN_POWER_H
#define GREEN_POWER_H

#include <array>
#include <deconz.h>

#define GREEN_POWER_CLUSTER_ID  0x0021
#define GREEN_POWER_ENDPOINT    0xf2
#define GP_SECURITY_KEY_SIZE 16
#define GP_MAX_PROXY_PAIRINGS 3
#define GP_DEFAULT_PROXY_GROUP 0xdd09

#if DECONZ_LIB_VERSION < 0x011000
  #define DBG_ZGP DBG_INFO  // DBG_ZGP didn't exist before version v2.8.x
#endif

using GpKey_t = std::array<unsigned char, GP_SECURITY_KEY_SIZE>;

GpKey_t GP_DecryptSecurityKey(quint32 sourceID, const GpKey_t &securityKey);
bool GP_SendProxyCommissioningMode(deCONZ::ApsController *apsCtrl, quint8 zclSeqNo);
bool GP_SendPairing(quint32 gpdSrcId, quint16 sinkGroupId, quint8 deviceId, quint32 frameCounter, const GpKey_t &key, deCONZ::ApsController *apsCtrl, quint8 zclSeqNo, quint16 gppShortAddress);

#endif // GREEN_POWER_H
