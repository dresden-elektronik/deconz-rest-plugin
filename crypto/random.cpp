/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <random>
#include <stdlib.h>
#include "random.h"

// OpenSSL reference to RAND_bytes()
typedef int (*RAND_bytes_t)(unsigned char *buf, int num);
static RAND_bytes_t RAND_bytes = nullptr;

#ifdef __linux__
#include <dlfcn.h>

/*! RAII helper to open/close OpenSSL.
 */
class RNGLib
{
public:
    RNGLib()
    {
        handle = dlopen("libcrypto.so", RTLD_LAZY);

        if (handle)
        {
            RAND_bytes = reinterpret_cast<RAND_bytes_t>(dlsym(handle, "RAND_bytes"));
        }
    }

    ~RNGLib()
    {
        RAND_bytes = nullptr;
        if (handle)
        {
            dlclose(handle);
        }
    }

private:
    void *handle = nullptr;
};
#endif

#ifdef __APPLE__
class RNGLib
{
};
#endif

#ifdef _WIN32
#include <windows.h>
/*! RAII helper to open/close OpenSSL.
 */
class RNGLib
{
public:
    RNGLib()
    {
        handle = LoadLibraryA("libcrypto-1_1.dll");

        if (handle)
        {
            RAND_bytes = reinterpret_cast<RAND_bytes_t>(GetProcAddress(handle, "RAND_bytes"));
        }
    }

    ~RNGLib()
    {
        RAND_bytes = nullptr;
        if (handle)
        {
            FreeLibrary(handle);
        }
    }

private:
    HMODULE handle = nullptr;
};
#endif

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
    RNGLib lib;

    if (RAND_bytes && RAND_bytes(buf, int(size)) == 1)
    {
        return;
    }

    fallbackRandom(buf, size);
}
