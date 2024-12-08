/*
 * Copyright (c) 2021-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <random>
#include "random.h"
#include "deconz/u_library_ex.h"

// OpenSSL reference to RAND_bytes()
typedef int (*RAND_bytes_t)(unsigned char *buf, int num);
static RAND_bytes_t RAND_bytes = nullptr;

/*! Fallback to C++ random number generator if OpenSSL isn't available.
 */
void fallbackRandom(unsigned char *buf, unsigned int size)
{
   std::random_device rd;
   std::uniform_int_distribution<int> dist(0, 255); //range is 0-255

   for (unsigned i = 0; i < size; i++)
   {
       buf[i] = dist(rd) & 0xFF;
   }
}


/*! Creates cryptographic secure random bytes.
 */
void CRYPTO_RandomBytes(unsigned char *buf, unsigned int size)
{
    if (!RAND_bytes)
    {
        void *libCrypto = U_library_open_ex("libcrypto");
        if (libCrypto)
        {
            RAND_bytes = reinterpret_cast<RAND_bytes_t>(U_library_symbol(libCrypto, "RAND_bytes"));
        }
    }

    if (RAND_bytes && RAND_bytes(buf, int(size)) == 1)
    {
        return;
    }

    fallbackRandom(buf, size);
}
