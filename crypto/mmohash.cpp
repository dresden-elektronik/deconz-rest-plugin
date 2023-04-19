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

bool getMmoHashFromInstallCode(std::string hexString, std::vector<unsigned char> &result)
{
#ifdef Q_OS_WIN
    QLibrary libCrypto(QLatin1String("libcrypto-1_1.dll"));
#else
    QLibrary libCrypto(QLatin1String("crypto"));
#endif

    lib_AES_set_encrypt_key = reinterpret_cast<int (*)(const unsigned char *userKey, const int bits, AES_KEY *key)>(libCrypto.resolve("AES_set_encrypt_key"));
    lib_AES_encrypt = reinterpret_cast<void (*)(const unsigned char *in, unsigned char *out, const AES_KEY *key)>(libCrypto.resolve("AES_encrypt"));

    if (!lib_AES_set_encrypt_key || !lib_AES_encrypt)
    {
        return false;
    }

    std::vector<unsigned char> data;

    for (std::string::size_type i = 0; i < hexString.length(); i += 2)
    {
        std::string byteString = hexString.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoul(byteString, nullptr, 16));
        data.push_back(byte);
    }

    unsigned char hashResult[AES_BLOCK_SIZE] = {0};
    unsigned char temp[AES_BLOCK_SIZE] = {0};
    unsigned hashResultLength = 0;
    unsigned lengthRemaining = 0;
    unsigned dataLength = data.size();

    if (data.size() > 0)
    {
        lengthRemaining = dataLength & (AES_BLOCK_SIZE - 1);

        if (dataLength >= AES_BLOCK_SIZE)
        {
            aesMmoHash(hashResultLength, hashResult, &data[0], dataLength);
        }
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
