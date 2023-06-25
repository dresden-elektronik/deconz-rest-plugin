#include "crypto/mmohash.h"

#ifdef HAS_OPENSSL

#include <QLibrary>
#include <openssl/evp.h>

#define AES_BLOCK_SIZE 16

static EVP_CIPHER_CTX *(*lib_EVP_CIPHER_CTX_new)(void);
static void (*lib_EVP_EncryptInit)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type, const unsigned char *key, const unsigned char *iv);
static int (*lib_EVP_EncryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl);
static int (*lib_EVP_EncryptFinal_ex)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);
static void (*lib_EVP_CIPHER_CTX_free)(EVP_CIPHER_CTX *ctx);
static const EVP_CIPHER *(*lib_EVP_aes_128_ecb)(void);

static bool aesMmoHash(unsigned char *result, unsigned char *data, unsigned dataSize)
{
    EVP_CIPHER_CTX *lib_ctx;

    while (dataSize >= AES_BLOCK_SIZE)
    {
        lib_ctx = lib_EVP_CIPHER_CTX_new();
        if (!lib_ctx)
            return false;

        lib_EVP_EncryptInit(lib_ctx, lib_EVP_aes_128_ecb(), result, NULL);

        unsigned char block[AES_BLOCK_SIZE];
        unsigned char encrypted_block[AES_BLOCK_SIZE * 2] = {0};

        memcpy(&block[0], &data[0], AES_BLOCK_SIZE);

        int outlen = 0;
        if (lib_EVP_EncryptUpdate(lib_ctx, &encrypted_block[0], &outlen, &block[0], AES_BLOCK_SIZE) != 1)
            return false;
        if (lib_EVP_EncryptFinal_ex(lib_ctx, &encrypted_block[0] + outlen, &outlen) != 1)
            return false;

        for (int i = 0; i < AES_BLOCK_SIZE; i++)
        {
            result[i] = encrypted_block[i] ^ block[i];
        }

        data += AES_BLOCK_SIZE;
        dataSize -= AES_BLOCK_SIZE;

        lib_EVP_CIPHER_CTX_free(lib_ctx);
        lib_ctx = 0;
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
bool CRYPTO_GetMmoHashFromInstallCode(const std::string &hexString, std::vector<unsigned char> &result)
{
#ifdef Q_OS_WIN
    QLibrary libCrypto(QLatin1String("libcrypto-1_1.dll"));
#else
    QLibrary libCrypto("crypto");
#endif

    lib_EVP_CIPHER_CTX_new = (EVP_CIPHER_CTX *(*)(void))libCrypto.resolve("EVP_CIPHER_CTX_new");
    lib_EVP_EncryptInit = (void (*)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type, const unsigned char *key, const unsigned char *iv))libCrypto.resolve("EVP_EncryptInit");
    lib_EVP_EncryptUpdate = (int (*)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl))libCrypto.resolve("EVP_EncryptUpdate");
    lib_EVP_EncryptFinal_ex = (int (*)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl))libCrypto.resolve("EVP_EncryptFinal_ex");
    lib_EVP_CIPHER_CTX_free = (void (*)(EVP_CIPHER_CTX *ctx))libCrypto.resolve("EVP_CIPHER_CTX_free");
    lib_EVP_aes_128_ecb = (const EVP_CIPHER *(*)(void))libCrypto.resolve("EVP_aes_128_ecb");

    if (!lib_EVP_CIPHER_CTX_new || !lib_EVP_EncryptInit || !lib_EVP_EncryptUpdate || !lib_EVP_EncryptFinal_ex || !lib_EVP_CIPHER_CTX_free | !lib_EVP_aes_128_ecb)
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
    unsigned pos;

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
    unsigned moreDataLength = dataLength;

    for (pos = 0; moreDataLength >= AES_BLOCK_SIZE;)
    {
        aesMmoHash(hashResult, &data[pos], dataLength);
        pos += AES_BLOCK_SIZE;
        moreDataLength -= AES_BLOCK_SIZE;
    }

    for (unsigned i = 0; i < moreDataLength; i++)
    {
        temp[i] = data[pos + i];
    }

    temp[moreDataLength] = 0x80;


    if (AES_BLOCK_SIZE - moreDataLength < 3)
    {
        aesMmoHash(hashResult, &temp[0], AES_BLOCK_SIZE);
        memset(&temp[0], 0x00, sizeof(temp));
    }

    temp[AES_BLOCK_SIZE - 2] = (dataLength >> 5) & 0xFF;
    temp[AES_BLOCK_SIZE - 1] = (dataLength << 3) & 0xFF;

    aesMmoHash(hashResult, &temp[0], AES_BLOCK_SIZE);

    result.resize(AES_BLOCK_SIZE);
    for (unsigned i = 0; i < AES_BLOCK_SIZE; i++)
    {
        result[i] = hashResult[i];
    }

    return true;
}

#else // ! HAS_OPENSSL

bool CRYPTO_GetMmoHashFromInstallCode(const std::string &hexString, std::vector<unsigned char> &result)
{
    (void)hexString;
    (void)result;
    return false;
}

#endif // HAS_OPENSSL
