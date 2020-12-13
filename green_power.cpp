#include <array>
#ifdef HAS_OPENSSL
#include <openssl/aes.h>
#include <openssl/evp.h>
#endif
#include <string>
#include "green_power.h"

// this code is based on
// https://github.com/Smanar/Zigbee_firmware/blob/master/Encryption.cpp

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

    // buffers for encryption and decryption
    constexpr size_t encryptLength = ((GP_SECURITY_KEY_SIZE + AES_BLOCK_SIZE) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    std::array<unsigned char, encryptLength> encryptBuf = { 0 };

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int outlen = 0;

    /* Set cipher type and mode */
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ccm(), NULL, NULL, NULL);

    /* Set nonce length if default 96 bits is not appropriate */
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, sizeof(nonce), NULL);

    /* Initialise key and IV */
    EVP_EncryptInit_ex(ctx, NULL, NULL, defaultTCLinkKey, nonce);

    /* Encrypt plaintext: can only be called once */
    EVP_EncryptUpdate(ctx, encryptBuf.data(), &outlen, securityKey.data(), static_cast<int>(securityKey.size()));
    EVP_CIPHER_CTX_free(ctx);

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
