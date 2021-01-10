#include <QLibrary>
#include <array>
#ifdef HAS_OPENSSL
#include <openssl/aes.h>
#include <openssl/evp.h>
#endif
#include <string>
#include "green_power.h"

// this code is based on
// https://github.com/Smanar/Zigbee_firmware/blob/master/Encryption.cpp
#define OPEN_SSL_VERSION_MIN 0x10100000

#define AES_KEYLENGTH 128
#define AES_BLOCK_SIZE 16

const unsigned char defaultTCLinkKey[] = { 0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39 };

// From https://github.com/Koenkk/zigbee-herdsman/blob/master/src/controller/greenPower.ts
/*!
 */
GpKey_t GP_DecryptSecurityKey(quint32 sourceID, const GpKey_t &securityKey)
{
    GpKey_t result = { 0 };

#ifdef HAS_OPENSSL

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

#ifdef Q_OS_WIN
    QLibrary libCrypto(QLatin1String("libcrypto-1_1.dll"));
    QLibrary libSsl(QLatin1String("libssl-1_1.dll"));
#else
    QLibrary libCrypto(QLatin1String("crypto"));
    QLibrary libSsl(QLatin1String("ssl"));
#endif

    if (!libCrypto.load() || !libSsl.load())
    {
        DBG_Printf(DBG_INFO, "OpenSSl library for ZGP encryption not found\n");
        return result;
    }

    unsigned long openSslVersion = 0;

    auto _OpenSSL_version_num = reinterpret_cast<unsigned long (*)(void)>(libCrypto.resolve("OpenSSL_version_num"));
    const auto _EVP_CIPHER_CTX_new = reinterpret_cast<EVP_CIPHER_CTX*(*)(void)>(libCrypto.resolve("EVP_CIPHER_CTX_new"));
    const auto _EVP_EncryptInit_ex = reinterpret_cast<int (*)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl, const unsigned char *key, const unsigned char *iv)>(libCrypto.resolve("EVP_EncryptInit_ex"));
    const auto _EVP_CIPHER_CTX_ctrl = reinterpret_cast<int (*)(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)>(libCrypto.resolve("EVP_CIPHER_CTX_ctrl"));
    const auto _EVP_EncryptUpdate = reinterpret_cast<int (*)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl)>(libCrypto.resolve("EVP_EncryptUpdate"));
    const auto _EVP_CIPHER_CTX_free = reinterpret_cast<void (*)(EVP_CIPHER_CTX *c)>(libCrypto.resolve("EVP_CIPHER_CTX_free"));
    const auto _EVP_aes_128_ccm  = reinterpret_cast<const EVP_CIPHER *(*)(void)>(libCrypto.resolve("EVP_aes_128_ccm"));

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
        DBG_Printf(DBG_INFO, "OpenSSl version 0x%08X loaded\n", openSslVersion);
    }
    else
    {
        DBG_Printf(DBG_INFO, "OpenSSl library version 0x%08X for ZGP encryption resolve symbols failed\n", openSslVersion);
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

#endif // HAS_OPENSSL

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
    req.setTxOptions(nullptr);
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
        DBG_Printf(DBG_INFO, "send GP proxy commissioning mode\n");
        return true;
    }

    DBG_Printf(DBG_INFO, "send GP proxy commissioning mode failed\n");
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
    req.setTxOptions(nullptr);
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
        quint8 options0 = 0xc8; // bits 0..7: add sink, enter commissioning mode, exit on window expire

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
        DBG_Printf(DBG_INFO, "send GP pairing to 0x%04X\n", gppShortAddress);
        return true;
    }

    DBG_Printf(DBG_INFO, "send GP pairing to 0x%04X failed\n", gppShortAddress);
    return false;
}
