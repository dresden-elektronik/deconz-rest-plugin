#ifndef HAS_OPENSSL
  #error "OpenSSL required"
#endif

#include <openssl/aes.h>
#include <vector>
#include <string>
#include <QByteArray>
#include <QLibrary>

static int (*lib_AES_set_encrypt_key)(const unsigned char *userKey, const int bits, AES_KEY *key);
static void (*lib_AES_encrypt)(const unsigned char *in, unsigned char *out, const AES_KEY *key);

// Below 2 functions are an adaptation/port from https://github.com/zigpy/zigpy/blob/dev/zigpy/util.py (aes_mmo_hash_update() and aes_mmo_hash())
static bool aesMmoHash(unsigned &length, unsigned char *result, unsigned char *data, unsigned dataSize)
{
    while (dataSize >= AES_BLOCK_SIZE)
    {
        AES_KEY aes_key;
        lib_AES_set_encrypt_key(result, 128, &aes_key);

        unsigned char block[AES_BLOCK_SIZE];
        unsigned char encrypted_block[AES_BLOCK_SIZE];

        memcpy(&block[0], &data[0], AES_BLOCK_SIZE);

        lib_AES_encrypt(&block[0], &encrypted_block[0], &aes_key);

        for (int i = 0; i < AES_BLOCK_SIZE; i++)
        {
            result[i] = encrypted_block[i] ^ block[i];
        }

        data += AES_BLOCK_SIZE;
        dataSize -= AES_BLOCK_SIZE;
        length += AES_BLOCK_SIZE;
    }

    return true;
}

#define CRC_POLY 0x8408
static unsigned short ccit_crc16(unsigned char *data_p, unsigned short length)
{
    unsigned char i;
    unsigned int data;
    unsigned long crc;

    crc = 0xFFFF;

    if (length == 0)
        return (~crc);

    do {
        for (i = 0, data = (unsigned int)0xff & *data_p++;
             i < 8;
             i++, data >>= 1) {
            if ((crc & 0x0001) ^ (data & 0x0001))
                crc = (crc >> 1) ^ CRC_POLY;
            else
                crc >>= 1;
        }
    } while (--length);

    crc = ~crc;

    return crc;
}

/*
    Verification of offical alliance example:

    curl -XPUT -H "Content-type: application/json" -d '{"installcode": "83FED3407A939723A5C639FF4C12"}' '127.0.0.1/api/12345/devices/999/installcode'

    [
      {
        "success": {
          "installcode": "83FED3407A939723A5C639FF4C12",
          "mmohash": "58C1828CF7F1C3FE29E7B1024AD84BFA"
        }
      }
    ]

    TODO(mpi): add more test cases for all IC lengths and test code
*/
bool getMmoHashFromInstallCode(const std::string &hexString, std::vector<unsigned char> &result)
{
#ifdef Q_OS_WIN
    QLibrary libCrypto(QLatin1String("libcrypto-1_1.dll"));
#else
    QLibrary libCrypto("crypto", "1.1");
#endif

    lib_AES_set_encrypt_key = reinterpret_cast<int (*)(const unsigned char *userKey, const int bits, AES_KEY *key)>(libCrypto.resolve("AES_set_encrypt_key"));
    lib_AES_encrypt = reinterpret_cast<void (*)(const unsigned char *in, unsigned char *out, const AES_KEY *key)>(libCrypto.resolve("AES_encrypt"));

    if (!lib_AES_set_encrypt_key || !lib_AES_encrypt)
    {
        return false;
    }

    if ((hexString.length() & 1) == 1)
    {
        return false; // must be even number
    }

    unsigned icLength = (unsigned)hexString.length() / 2;
    // test valid IC sizes (plus 2 bytes for CRC)
    if (!(icLength == (6 + 2) || icLength == (8 + 2) || icLength == (12 + 2) || icLength == (16 + 2)))
    {
        return false; // valid IC sizes according https://wiki.st.com/stm32mcu/wiki/Connectivity:Zigbee_Install_Code
    }

    unsigned char data[16 + 2]; // IC + CRC16
    unsigned dataLength = 0;

    for (unsigned i = 0; i + 1 < icLength * 2; i += 2)
    {
        char ch = 0;
        unsigned char byte = 0;

        for (int j = 0; j < 2; j++)
        {
            ch = hexString.at(i + j);

            if      (ch >= '0' && ch <= '9') { ch = ch - '0'; }
            else if (ch >= 'A' && ch <= 'F') { ch = ch - 'A' + 10; }
            else if (ch >= 'a' && ch <= 'f') { ch = ch - 'a' + 10; }
            else
            {
                return false; // not a hex digit
            }

            byte <<= 4;
            byte |= ch & 0xF;
        }

        data[dataLength] = byte;
        dataLength++;
    }

    {   // Check that crc is correct. There have been devices out
        // there where the crc is invalid or byte-swapped.
        // Don't fail in that case but auto correct.
        unsigned short crc = ccit_crc16(&data[0], dataLength - 2);
        unsigned char crc_lo = crc & 0xFF;
        unsigned char crc_hi = (crc >> 8) & 0xFF;

        if (data[dataLength - 1] != crc_hi || data[dataLength - 2] != crc_lo)
        {
            data[dataLength - 1] = crc_hi;
            data[dataLength - 2] = crc_lo;
        }
    }

    unsigned char hashResult[AES_BLOCK_SIZE] = {0};
    unsigned char temp[AES_BLOCK_SIZE] = {0};
    unsigned hashResultLength = 0;
    unsigned lengthRemaining = 0;

    lengthRemaining = dataLength & (AES_BLOCK_SIZE - 1);

    if (dataLength >= AES_BLOCK_SIZE)
    {
        aesMmoHash(hashResultLength, hashResult, &data[0], dataLength);
    }

    for (unsigned i = 0; i < lengthRemaining; i++)
    {
        temp[i] = data[i];
    }

    temp[lengthRemaining] = 0x80;
    hashResultLength += lengthRemaining;

    if (AES_BLOCK_SIZE - lengthRemaining < 3)
    {
        aesMmoHash(hashResultLength, hashResult, &temp[0], AES_BLOCK_SIZE);
        hashResultLength -= AES_BLOCK_SIZE;
        memset(&temp[0], 0x00, sizeof(temp));
    }

    uint bit_size = hashResultLength * 8;
    temp[AES_BLOCK_SIZE - 2] = (bit_size >> 8) & 0xFF;
    temp[AES_BLOCK_SIZE - 1] = (bit_size) & 0xFF;

    aesMmoHash(hashResultLength, hashResult, &temp[0], AES_BLOCK_SIZE);

    result.resize(AES_BLOCK_SIZE);
    for (unsigned i = 0; i < AES_BLOCK_SIZE; i++)
    {
        result[i] = hashResult[i];
    }

    return true;
}
