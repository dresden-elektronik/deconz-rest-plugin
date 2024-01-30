/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "password.h"
#include <deconz/dbg_trace.h>
#include <deconz/u_library.h>

typedef char *(*lib_crypt_fn)(const char *phrase, const char *setting);
static lib_crypt_fn crypt_fn = nullptr;

// TODO(mpi): CRYPTO_ScryptPassword() is much stronger and available on all platforms.
//            Need to figure out a upgrade path.

/*! Encrypts a string with using crypt() MD5 + salt. (unix only)
    \param str the input string
    \return the encrypted string on success or the unchanged input string on fail
 */
std::string CRYPTO_EncryptGatewayPassword(const std::string &str)
{
#ifndef _WIN32
    const char *pwsalt = "$1$8282jdkmskwiu29291"; // $1$ for MD5

    if (!crypt_fn)
    {
        void *lib = U_library_open("libcrypt");

        if (lib)
        {
            crypt_fn = reinterpret_cast<lib_crypt_fn>(U_library_symbol(lib, "crypt"));
        }
        else
        {
            DBG_Printf(DBG_ERROR, "failed to load libcrypt\n");
        }
    }

    // encrypt and salt the hash
    if (crypt_fn)
    {
        const char *enc = crypt_fn(str.c_str(), pwsalt);

        if (enc)
        {
            return enc;
        }
        // else: fall through and return str
    }

#endif // ! _WIN32
    return str;
}
