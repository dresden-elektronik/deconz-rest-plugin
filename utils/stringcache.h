/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef STRING_CACHE_H
#define STRING_CACHE_H

#include <utils/bufstring.h>

/*! \class StringCache

    A static string cache with zero dynamic memory allocations.
    Immutable read only strings are only added to the cache but never removed.
    Mutable string can be used for things like per resource names.
*/
class StringCache
{
public:
    enum Mode {Mutable, Immutable};

    /*! Adds a string if not already exists when \p mode is \c Immutable.
        Adding the same immutable string multiple times will only reference the same slot
        in the cache (deduplication).

        \returns A valid handle when the entry is added or already exists.
        \returns Invalid handle if the string is too large or the cache is full.
     */
    BufStringCacheHandle put(const char *str, size_t length, Mode mode);

private:
    // BufStringCache<32, 1024> mutable32; // for names and other strings
    BufStringCache<32, 1024> immutable32;
    BufStringCache<64, 1024> immutable64;
    BufStringCache<128, 512> immutable128;
};

/*! Returns pointer to the global string cache.
 */
StringCache *GlobalStringCache();

#endif // STRING_CACHE_H
