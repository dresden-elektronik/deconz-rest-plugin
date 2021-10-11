/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <utils/stringcache.h>

static StringCache stringCache_{};

StringCache *GlobalStringCache()
{
    return &stringCache_;
}

BufStringCacheHandle StringCache::put(const char *str, size_t length, Mode mode)
{
    if (mode == Immutable)
    {
        if (length <= immutable32.maxStringSize())
        {
            return immutable32.put(str, length);
        }
        else if (length <= immutable64.maxStringSize())
        {
            return immutable64.put(str, length);
        }
        else if (length <= immutable128.maxStringSize())
        {
            return immutable128.put(str, length);
        }
        else
        {
            Q_ASSERT(length < immutable128.maxStringSize()); // too large, not supported
        }
    }
    else if (mode == Mutable)
    {
        Q_ASSERT(0); // TODO implement
    }

    return {};
}
