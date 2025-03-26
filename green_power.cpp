/*
 * Copyright (c) 2020-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QDataStream>

#ifdef HAS_OPENSSL
#define OPEN_SSL_VERSION_MIN 0x10100000L

#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= OPEN_SSL_VERSION_MIN
  #define HAS_RECENT_OPENSSL
#endif
#endif

#ifdef HAS_RECENT_OPENSSL
  #include <openssl/aes.h>
  #include <openssl/evp.h>
#endif
#include <string>
#include "deconz/aps_controller.h"
#include "deconz/dbg_trace.h"
#include "deconz/green_power.h"
#include "deconz/zcl.h"
#include "deconz/u_library_ex.h"
#include "green_power.h"
#include "resource.h"

#define GP_PAIR_INTERVAL_SECONDS (60 * 15)

// this code is based on
// https://github.com/Smanar/Zigbee_firmware/blob/master/Encryption.cpp

#define AES_KEYLENGTH 128
#define AES_BLOCK_SIZE 16


// From https://github.com/Koenkk/zigbee-herdsman/blob/master/src/controller/greenPower.ts
/*!
 */
GpKey_t GP_DecryptSecurityKey(quint32 sourceID, const GpKey_t &securityKey)
{
    GpKey_t result = { 0 };

#ifdef HAS_RECENT_OPENSSL
    void *libCrypto = nullptr;
    void *libSsl = nullptr;
    const unsigned char defaultTCLinkKey[] = { 0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39 };

    unsigned char nonce[13]; // u8 source address, u32 frame counter, u8 security control
    unsigned char sourceIDInBytes[4];

    sourceIDInBytes[0] = sourceID & 0x000000ff;
    sourceIDInBytes[1] = (sourceID & 0x0000ff00) >> 8;
    sourceIDInBytes[2] = (sourceID & 0x00ff0000) >> 16;
    sourceIDInBytes[3] = (sourceID & 0xff000000) >> 24;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            nonce[4 * i + j] = sourceIDInBytes[j];
        }
    }

    nonce[12] = 0x05;

    libCrypto = U_library_open_ex("libcrypto");
    libSsl = U_library_open_ex("libssl");

    if (!libCrypto || !libSsl)
    {
        DBG_Printf(DBG_ZGP, "[ZGP] OpenSSl library for ZGP encryption not found\n");
        return result;
    }

    unsigned long openSslVersion = 0;

    auto _OpenSSL_version_num = reinterpret_cast<unsigned long (*)(void)>(U_library_symbol(libCrypto, "OpenSSL_version_num"));
    const auto _EVP_CIPHER_CTX_new = reinterpret_cast<EVP_CIPHER_CTX*(*)(void)>(U_library_symbol(libCrypto, "EVP_CIPHER_CTX_new"));
    const auto _EVP_EncryptInit_ex = reinterpret_cast<int (*)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl, const unsigned char *key, const unsigned char *iv)>(U_library_symbol(libCrypto, "EVP_EncryptInit_ex"));
    const auto _EVP_CIPHER_CTX_ctrl = reinterpret_cast<int (*)(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)>(U_library_symbol(libCrypto, "EVP_CIPHER_CTX_ctrl"));
    const auto _EVP_EncryptUpdate = reinterpret_cast<int (*)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl)>(U_library_symbol(libCrypto, "EVP_EncryptUpdate"));
    const auto _EVP_CIPHER_CTX_free = reinterpret_cast<void (*)(EVP_CIPHER_CTX *c)>(U_library_symbol(libCrypto, "EVP_CIPHER_CTX_free"));
    const auto _EVP_aes_128_ccm  = reinterpret_cast<const EVP_CIPHER *(*)(void)>(U_library_symbol(libCrypto, "EVP_aes_128_ccm"));

    if (_OpenSSL_version_num)
    {
        openSslVersion = _OpenSSL_version_num();
    }

    if (openSslVersion >= OPEN_SSL_VERSION_MIN &&
            _EVP_CIPHER_CTX_new &&
            _EVP_EncryptInit_ex &&
            _EVP_CIPHER_CTX_ctrl &&
            _EVP_EncryptUpdate &&
            _EVP_CIPHER_CTX_free &&
            _EVP_aes_128_ccm)
    {
        DBG_Printf(DBG_ZGP, "[ZGP] OpenSSl version 0x%08lX loaded\n", openSslVersion);
    }
    else
    {
        DBG_Printf(DBG_ZGP, "[ZGP] OpenSSl library version 0x%08lX for ZGP encryption resolve symbols failed\n", openSslVersion);
        return result;
    }

    // buffers for encryption and decryption
    constexpr size_t encryptLength = ((GP_SECURITY_KEY_SIZE + AES_BLOCK_SIZE) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    std::array<unsigned char, encryptLength> encryptBuf = { 0 };

    EVP_CIPHER_CTX *ctx = _EVP_CIPHER_CTX_new();
    int outlen = 0;

    /* Set cipher type and mode */
    _EVP_EncryptInit_ex(ctx, _EVP_aes_128_ccm(), nullptr, nullptr, nullptr);

    /* Set nonce length if default 96 bits is not appropriate */
    _EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, sizeof(nonce), nullptr);

    /* Initialise key and IV */
    _EVP_EncryptInit_ex(ctx, nullptr, nullptr, defaultTCLinkKey, nonce);

    /* Encrypt plaintext: can only be called once */
    _EVP_EncryptUpdate(ctx, encryptBuf.data(), &outlen, securityKey.data(), static_cast<int>(securityKey.size()));
    _EVP_CIPHER_CTX_free(ctx);

    std::copy(encryptBuf.begin(), encryptBuf.begin() + result.size(), result.begin());

#else
    Q_UNUSED(securityKey)
    DBG_Printf(DBG_ERROR, "[ZGP] failed to decrypt GPDKey for 0x%08X, OpenSSL is not available or too old\n", unsigned(sourceID));
#endif // HAS_RECENT_OPENSSL

    return result;
}

/*! Send Commissioning Mode command to GP proxy device.
 */
bool GP_SendProxyCommissioningMode(deCONZ::ApsController *apsCtrl, quint8 zclSeqNo)
{
    deCONZ::ApsDataRequest req;

    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setNwk(deCONZ::BroadcastRouters);
    req.setProfileId(GP_PROFILE_ID);
    req.setClusterId(GREEN_POWER_CLUSTER_ID);
    req.setDstEndpoint(GREEN_POWER_ENDPOINT);
    req.setSrcEndpoint(GREEN_POWER_ENDPOINT);
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
    req.setTxOptions(nullptr);
#endif
    req.setRadius(0);

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclSeqNo);
    zclFrame.setCommandId(0x02); // commissioning mode
    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 options = 0x0b; // enter commissioning mode, exit on window expire
        quint16 window = 40;
        stream << options;
        stream << window;
    }

    { // ZCL frame

        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    // broadcast
    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        DBG_Printf(DBG_ZGP, "[ZGP] send GP proxy commissioning mode\n");
        return true;
    }

    DBG_Printf(DBG_ZGP, "[ZGP] send GP proxy commissioning mode failed\n");
    return false;
}

/*! Send Pair command to GP proxy device.
 */
bool GP_SendPairing(quint32 gpdSrcId, quint16 sinkGroupId, quint8 deviceId, quint32 frameCounter, const GpKey_t &key, deCONZ::ApsController *apsCtrl, quint8 zclSeqNo, quint16 gppShortAddress)
{
    deCONZ::ApsDataRequest req;

    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setNwk(gppShortAddress);
    req.setProfileId(GP_PROFILE_ID);
    req.setClusterId(GREEN_POWER_CLUSTER_ID);
    req.setDstEndpoint(GREEN_POWER_ENDPOINT);
    req.setSrcEndpoint(GREEN_POWER_ENDPOINT);
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
    req.setTxOptions(nullptr);
#endif
    req.setRadius(0);

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclSeqNo);
    zclFrame.setCommandId(0x01); // pairing
    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        // 0..2: applicationID
        // 3: add sink
        // 4: remove gpd
        // 5..6: communication mode
        // 7: gpd fixed
        quint8 options0 = 0x48; // bits 0..7: add sink, enter commissioning mode, exit on window expire

        // 0 / 8: gpd mac seq number capabilities
        // 1..2 / 9..10: security level
        // 3..5 / 11..13: security key type
        // 6 / 14: frame counter present
        // 7 / 15: gpd security key present

        // The GPDsecurityFrameCounter field shall be present whenever the AddSink sub-field of the Options field is set to 0b1;

        quint8 options1 = 0xe5;
        // bits 8..15: security level 0b10 (Full (4B) frame counter and full (4B) MIC only)
        //             key type 0b100 (individual, out- of-the-box GPD key),
        //             frame counter present, security key present
        quint8 options2 = 0x00;
        stream << options0;
        stream << options1;
        stream << options2;
        stream << gpdSrcId;
        stream << sinkGroupId;
        stream << deviceId;
        stream << frameCounter;

        {
            for (size_t  i = 0; i < 16; i++)
            {
                stream << key[i];
            }
        }
    }

    { // ZCL frame

        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    // broadcast
    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        DBG_Printf(DBG_ZGP, "[ZGP]  send GP pairing to 0x%04X\n", gppShortAddress);
        return true;
    }

    DBG_Printf(DBG_ZGP, "[ZGP] send GP pairing to 0x%04X failed\n", gppShortAddress);
    return false;
}


// TODO remove TMP_ functions after alarm systems PR is merged, where these functions are in utils.h
static bool TMP_isHexChar(char ch)
{
    return ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'));
}

static quint64 TMP_extAddressFromUniqueId(const QString &uniqueId)
{
    quint64 result = 0;

    if (uniqueId.size() < 23)
    {
        return result;
    }

    // 28:6d:97:00:01:06:41:79-01-0500  31 characters
    int pos = 0;
    char buf[16 + 1];

    for (auto ch : uniqueId)
    {
        if (ch != ':')
        {
            buf[pos] = ch.toLatin1();

            if (!TMP_isHexChar(buf[pos]))
            {
                return result;
            }
            pos++;
        }

        if (pos == 16)
        {
            buf[pos] = '\0';
            break;
        }
    }

    if (pos == 16)
    {
        result = strtoull(buf, nullptr, 16);
    }

    return result;
}

/*! For already paired ZGP devices a Pair command needs to be send periodically every \c GP_PAIR_INTERVAL_SECONDS
    in order to keep ZGP Proxy entrys alive.

    Each ZGP device keeps track of when the last Pair command was sent and the current device frame counter.
 */
bool GP_SendPairingIfNeeded(Resource *resource, deCONZ::ApsController *apsCtrl, quint8 zclSeqNo)
{
    if (!resource || !apsCtrl)
    {
        return false;
    }

    ResourceItem *gpdLastpair = resource->item(RStateGPDLastPair);
    if (!gpdLastpair)
    {
        return false;
    }

    const deCONZ::SteadyTimeRef now = deCONZ::steadyTimeRef();

    if (now - deCONZ::SteadyTimeRef{gpdLastpair->toNumber()} < deCONZ::TimeSeconds{GP_PAIR_INTERVAL_SECONDS})
    {
        return false;
    }

    // the GPDKey must be known to send pair command
    ResourceItem *gpdKey = resource->item(RConfigGPDKey);

    if (!gpdKey || gpdKey->toString().isEmpty())
    {
        return false;
    }

    ResourceItem *frameCounter = resource->item(RStateGPDFrameCounter);
    ResourceItem *gpdDeviceId = resource->item(RConfigGPDDeviceId);
    ResourceItem *uniqueId = resource->item(RAttrUniqueId);

    if (!gpdKey || !frameCounter || !gpdDeviceId || !uniqueId)
    {
        return false;
    }

    auto srcGpdId = TMP_extAddressFromUniqueId(uniqueId->toString());

    if (srcGpdId == 0 || srcGpdId > UINT32_MAX)
    {
        return false; // should not happen
    }

    GpKey_t key;

    {
        QByteArray arr = QByteArray::fromHex(gpdKey->toString().toLocal8Bit());
        DBG_Assert(arr.size() == int(key.size()));
        if (arr.size() != int(key.size()))
        {
            return false;
        }

        memcpy(key.data(), arr.constData(), key.size());
    }

    quint8 deviceId = gpdDeviceId->toNumber() & 0xFF;

    if (GP_SendPairing(quint32(srcGpdId), GP_DEFAULT_PROXY_GROUP, deviceId, frameCounter->toNumber(), key, apsCtrl, zclSeqNo, deCONZ::BroadcastRouters))
    {
        gpdLastpair->setValue(now.ref);
        return true;
    }

    return false;
}
