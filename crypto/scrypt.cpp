/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#define OPEN_SSL_VERSION_MIN 0x10101000L
#ifdef HAS_OPENSSL
  #include <openssl/opensslv.h>
  #if OPENSSL_VERSION_NUMBER < OPEN_SSL_VERSION_MIN // defined on supported versions in evp.h header
    #undef HAS_OPENSSL
  #endif
#endif

#include <array>
#include <QLibrary>
#include "random.h"
#include "scrypt.h"


#ifdef HAS_OPENSSL

#include <openssl/evp.h>
#include <openssl/kdf.h>

static int (*lib_EVP_PKEY_CTX_ctrl_uint64)(EVP_PKEY_CTX *ctx, int keytype, int optype, int cmd, uint64_t value);

static int wrap_EVP_PKEY_CTX_set_scrypt_N(EVP_PKEY_CTX *ctx, uint64_t n)
{
    return lib_EVP_PKEY_CTX_ctrl_uint64(ctx, -1, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_SCRYPT_N, n);
}

static int wrap_EVP_PKEY_CTX_set_scrypt_r(EVP_PKEY_CTX *ctx, uint64_t r)
{
    return lib_EVP_PKEY_CTX_ctrl_uint64(ctx, -1, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_SCRYPT_R, r);
}

static int wrap_EVP_PKEY_CTX_set_scrypt_p(EVP_PKEY_CTX *ctx, uint64_t p)
{
    return lib_EVP_PKEY_CTX_ctrl_uint64(ctx, -1, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_SCRYPT_P, p);
}

/*! KDF to scrypt the \p input.
 */
static int scryptDerive(const char *input, size_t inputLength, std::array<unsigned char, 64> &out, int N, int r, int p, const unsigned char *salt, size_t saltlen)
{
#ifdef Q_OS_WIN
    QLibrary libCrypto(QLatin1String("libcrypto-1_1.dll"));
    QLibrary libSsl(QLatin1String("libssl-1_1.dll"));
#else
    QLibrary libCrypto(QLatin1String("crypto"));
    QLibrary libSsl(QLatin1String("ssl"));
#endif

    unsigned long openSslVersion = 0;

    auto _OpenSSL_version_num = reinterpret_cast<unsigned long (*)(void)>(libCrypto.resolve("OpenSSL_version_num"));

    const auto lib_EVP_PKEY_CTX_new_id = reinterpret_cast<EVP_PKEY_CTX *(*)(int id, ENGINE *e)>(libCrypto.resolve("EVP_PKEY_CTX_new_id"));
    const auto lib_EVP_PKEY_derive_init = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx)>(libCrypto.resolve("EVP_PKEY_derive_init"));
    const auto lib_EVP_PKEY_CTX_ctrl = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, int keytype, int optype, int cmd, int p1, void *p2)>(libCrypto.resolve("EVP_PKEY_CTX_ctrl"));
    lib_EVP_PKEY_CTX_ctrl_uint64 = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, int keytype, int optype, int cmd, uint64_t value)>(libCrypto.resolve("EVP_PKEY_CTX_ctrl_uint64"));
    const auto lib_EVP_PKEY_derive = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen)>(libCrypto.resolve("EVP_PKEY_derive"));
    const auto lib_EVP_PKEY_CTX_free = reinterpret_cast<void (*)(EVP_PKEY_CTX *ctx)>(libCrypto.resolve("EVP_PKEY_CTX_free"));

    const auto lib_EVP_PKEY_CTX_set1_pbe_pass = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, const char *pass, int passlen)>(libCrypto.resolve("EVP_PKEY_CTX_set1_pbe_pass"));

    const auto lib_EVP_PKEY_CTX_set1_scrypt_salt = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, const unsigned char *salt, int saltlen)>(libCrypto.resolve("EVP_PKEY_CTX_set1_scrypt_salt"));

    auto lib_EVP_PKEY_CTX_set_scrypt_N = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, uint64_t n)>(libCrypto.resolve("EVP_PKEY_CTX_set_scrypt_N"));
    auto lib_EVP_PKEY_CTX_set_scrypt_r = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, uint64_t r)>(libCrypto.resolve("EVP_PKEY_CTX_set_scrypt_r"));
    auto lib_EVP_PKEY_CTX_set_scrypt_p = reinterpret_cast<int (*)(EVP_PKEY_CTX *ctx, uint64_t p)>(libCrypto.resolve("EVP_PKEY_CTX_set_scrypt_p"));

    if (_OpenSSL_version_num)
    {
        openSslVersion = _OpenSSL_version_num();
    }

    if (openSslVersion < OPEN_SSL_VERSION_MIN ||
            ! lib_EVP_PKEY_CTX_new_id ||
            ! lib_EVP_PKEY_derive_init ||
            ! lib_EVP_PKEY_CTX_ctrl ||
            ! lib_EVP_PKEY_CTX_ctrl_uint64 ||
            ! lib_EVP_PKEY_derive ||
            ! lib_EVP_PKEY_CTX_set1_pbe_pass ||
            ! lib_EVP_PKEY_CTX_set1_scrypt_salt ||
            ! lib_EVP_PKEY_CTX_free)
    {
        return -1;
    }

    if (! lib_EVP_PKEY_CTX_set_scrypt_N ||
        ! lib_EVP_PKEY_CTX_set_scrypt_r ||
        ! lib_EVP_PKEY_CTX_set_scrypt_p)
    {
        // OpenSSL 1.x has these as macros wrapping EVP_PKEY_CTX_ctrl_uint64
        lib_EVP_PKEY_CTX_set_scrypt_N = wrap_EVP_PKEY_CTX_set_scrypt_N;
        lib_EVP_PKEY_CTX_set_scrypt_r = wrap_EVP_PKEY_CTX_set_scrypt_r;
        lib_EVP_PKEY_CTX_set_scrypt_p = wrap_EVP_PKEY_CTX_set_scrypt_p;
    }

    int result = 0;
    EVP_PKEY_CTX *pctx;

    size_t outlen = out.size();
    pctx = lib_EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, NULL);

    result = pctx ? 0 : -1;

    if (result == 0 && lib_EVP_PKEY_derive_init(pctx) <= 0)
    {
        result = -1;
    }

    if (result == 0 && lib_EVP_PKEY_CTX_set1_pbe_pass(pctx, input, inputLength) <= 0)
    {
        result = -2;
    }

    if (result == 0 && lib_EVP_PKEY_CTX_set1_scrypt_salt(pctx, salt, saltlen) <= 0)
    {
        result = -3;
    }

    if (result == 0 && lib_EVP_PKEY_CTX_set_scrypt_N(pctx, N) <= 0)
    {
        result = -4;
    }

    if (result == 0 && lib_EVP_PKEY_CTX_set_scrypt_r(pctx, r) <= 0) {
        result = -5;
    }

    if (result == 0 && lib_EVP_PKEY_CTX_set_scrypt_p(pctx, p) <= 0)
    {
        result = -6;
    }

    if (result == 0 && lib_EVP_PKEY_derive(pctx, out.data(), &outlen) <= 0)
    {
        result = -7;
    }

    lib_EVP_PKEY_CTX_free(pctx);

    return result;
}

/*! Encrypts the \p input with scrypt and given parameters.

    The \p salt should be created with \c CRYPTO_GenerateSalt.

    \returns a PHC encoded password hash
 */
std::string CRYPTO_ScryptPassword(const std::string &input, const std::string &salt, int N, int r, int p)
{
    std::string res;

    if (input.empty() || salt.empty())
    {
        return res;
    }

    std::array<unsigned char, 64> out;
    // We coerce the salt value to an unsigned char *, as it is no longer a string past this point.
    if (scryptDerive(input.data(), input.size(), out, N, r, p, (const unsigned char*)salt.data(), salt.length()) != 0)
    {
        return res;
    }

    // PHC format
    // https://github.com/P-H-C/phc-string-format/blob/master/phc-sf-spec.md

    QByteArray base64Hash = QByteArray(reinterpret_cast<char*>(out.data()), out.size())
                              .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    QString s = QString("$scrypt$N=%1$r=%2$p=%3$%4$%5")
            .arg(N).arg(r).arg(p)
            .arg(salt.data(), base64Hash.constData());

    res = s.toStdString();

    return res;
}

/*! Parses the PHC encoded parameters, N, r, p, and salt, used for scrypt.
 */
bool CRYPTO_ParsePhcScryptParameters(const std::string &phcHash, ScryptParameters *param)
{
    if (!param || phcHash.empty() || strstr(phcHash.data(), "$scrypt") == nullptr)
    {
        return false;
    }

    const char *N = strstr(phcHash.data(), "$N=");
    const char *r = strstr(phcHash.data(), "$r=");
    const char *p = strstr(phcHash.data(), "$p=");

    if (!N || !r || !p)
    {
        return false;
    }

    const char *salt = strchr(p + 1, '$');

    if (!salt)
    {
        return false;
    }

    const char *saltEnd = strchr(salt + 1, '$');

    if (!saltEnd)
    {
        return false;
    }

    param->N = std::atoi(N + 3);
    param->r = std::atoi(r + 3);
    param->p = std::atoi(p + 3);
    param->salt = std::string(salt + 1, (saltEnd - salt) - 1);

    return param->N > 0 && param->r > 0 && param->p > 0 && !param->salt.empty();
}

/*! Returns true if the PHC encoded password hash matches the \p password.
 */
bool CRYPTO_ScryptVerify(const std::string &phcHash, const std::string &password)
{
    if (phcHash.empty() || password.empty())
    {
        return false;
    }

    ScryptParameters param;
    if (!CRYPTO_ParsePhcScryptParameters(phcHash, &param))
    {
        return false;
    }

    std::string hash2 = CRYPTO_ScryptPassword(password, param.salt, param.N, param.r, param.p);

    return hash2 == phcHash;
}

#else

// OpenSSL version too old
// return errors or empty results

bool CRYPTO_ParsePhcScryptParameters(const std::string &, ScryptParameters *)
{
    return false;
}

bool CRYPTO_ScryptVerify(const std::string &, const std::string &)
{
    return false;
}

std::string CRYPTO_ScryptPassword(const std::string &, const std::string &, int, int, int)
{
    return {};
}

#endif // HAS_OPENSSL

/*! Returns a base64 encoded cryptographic secure salt.
 */
std::string CRYPTO_GenerateSalt()
{
    unsigned char saltRandom[16]{0};
    CRYPTO_RandomBytes(saltRandom, sizeof(saltRandom));

    const auto salt = QByteArray::fromRawData(reinterpret_cast<char*>(saltRandom), sizeof(saltRandom))
                            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    return salt.toStdString();
}
