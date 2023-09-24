/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef BUF_STRING_H
#define BUF_STRING_H

#include <QString>
#include <cstddef>
#include <cstring>
#include <cassert>

bool startsWith(QLatin1String str, QLatin1String needle);

constexpr size_t BufStringOverHead = 2; // length + null termintor

/*! The data part in each BufString starts with \c BufStringBase.
    This allows embeddeding in handles pointing to arbitrary size BufStings.
 */
struct BufStringBase
{
    uint8_t length;
    char buf[1];
};

/*! A fixed size zero allocation string buffer.
 */
template <size_t Size>
class BufString
{
    union
    {
        BufStringBase base_;
        char buf[Size]{ };
    };

    static_assert (Size <= (255 + BufStringOverHead), "Size too large");

public:
    constexpr BufString()
    { clear(); }

    constexpr BufString(const char *str) :
        BufString(str, strlen(str))
    { }

    constexpr BufString(const char *str, const size_t len) :
        BufString()
    {
        setString(str, len);
    }

    BufString(const BufString &rhs)
    {
        setString(rhs.c_str(), rhs.size());
    }

    constexpr const BufStringBase *base() const { return &base_; }

    constexpr bool setString(const char *str)
    {
        assert(str);
        return setString(str, strlen(str));
    }

    constexpr bool setString(const char *str, const size_t len)
    {
#ifdef QT_DEBUG
        assert(str);
        assert(str != c_str());
        assert(len <= maxSize());
        assert(1 + len < Size);
#endif
        if (str == c_str())
        {
            return true;
        }

        if (len > maxSize())
        {
            return false;
        }

        buf[0] = len;
        if (len > 0)
        {
            memmove(&buf[1], str, len);
        }
        buf[1 + len] = '\0';
        assert(buf[1 + size()] == '\0');
        return true;
    }

    constexpr void clear()
    {
        buf[0] = 0;
        buf[1] = '\0';
    }

    constexpr const char *c_str() const
    {
        assert(size() < Size);
        assert(buf[1 + size()] == '\0');
        return &buf[1];
    };
    constexpr bool empty() const { return size() == 0; }
    constexpr size_t size() const { return buf[0]; }
    constexpr size_t maxSize() const { return Size - BufStringOverHead; }
    constexpr size_t capacity() const { return maxSize() - size(); }

    operator QString () const { return QString::fromUtf8(c_str(), int(size())); }
    operator QLatin1String () const { return QLatin1String(c_str(), int(size())); }

    template<size_t U>
    constexpr bool operator==(const BufString<U> &rhs) const
    {
        const size_t sz = size() + 1; // first byte is length

        for (size_t i = 0; i < sz; i++)
        {
            if (buf[i] != rhs.buf[i])
            {
                return false;
            }
        }

        return true;
    }

    constexpr BufString &operator=(const char *str)
    {
        setString(str);
        return *this;
    }

    BufString &operator=(const BufString &rhs)
    {
        assert(this != &rhs);
        assert(rhs.size() <= maxSize());
        if (rhs.size() <= maxSize())
        {
            setString(rhs.c_str(), rhs.size());
        }
        return *this;
    }

    bool startsWith(const QLatin1String &str) const
    {
        if (str.size() <= int(size()))
        {
            return ::startsWith(QLatin1String(c_str(), int(size())), str);
        }
        return false;
    }

    bool startsWith(const QString &str) const
    {
        if (str.size() <= size())
        {
            const QString q(c_str());
            return q.startsWith(str);
        }
        return false;
    }
};

template<size_t T, size_t U>
inline bool operator<(const BufString<T> &lhs, const BufString<U> &rhs)
{
    return strcmp(lhs.c_str(), rhs.c_str()) < 0;
}

template <size_t Size>
inline bool operator==(const BufString<Size> &lhs, const std::string &rhs)
{
    return strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

template <size_t Size>
inline bool operator!=(const BufString<Size> &lhs, const std::string &rhs)
{
    return !(lhs == rhs);
}

template <size_t Size>
inline bool operator==(const BufString<Size> &lhs, const QLatin1String &rhs)
{
    return rhs.size() != 0 && strcmp(lhs.c_str(), rhs.data()) == 0;
}

template <size_t Size>
inline bool operator!=(const BufString<Size> &lhs, const QLatin1String &rhs)
{
    return !(rhs == lhs);
}

template <size_t Size>
inline bool operator==(const BufString<Size> &lhs, const QString &rhs)
{
    return !rhs.isEmpty() && strcmp(lhs.c_str(), rhs.toUtf8().constData()) == 0;
}

template <size_t Size>
inline bool operator!=(const BufString<Size> &lhs, const QString &rhs)
{
    return !(rhs == lhs);
}

template <size_t Size>
bool operator==(const BufString<Size> &lhs, const char *rhs)
{
    return rhs && strcmp(lhs.c_str(), rhs) == 0;
}

template <size_t Size>
bool operator!=(const BufString<Size> &lhs, const char *rhs)
{
    return !(lhs == rhs);
}

template <size_t Size>
inline QLatin1String toLatin1String(const BufString<Size> &str)
{
    return QLatin1String(str.c_str(), int(str.size()));
}

/*! Lightweight handle into a \c BufStringCache.

    The string can be accessed read only via \c base pointer.
 */
struct BufStringCacheHandle
{
    const BufStringBase *base = nullptr;
    uint16_t cacheId = 0;
    uint16_t index = 0;
    uint16_t maxSize = 0;
};

/*! Returns true if handle points to a valid string cache entry.
 */
inline bool isValid(BufStringCacheHandle hnd)
{
    return hnd.cacheId != 0 && hnd.base != nullptr;
}

/*! A cache for BufStrings with deduplication.
 */
template <size_t Size, size_t NElements>
class BufStringCache
{
    size_t m_size = 0;
    BufString<Size> m_strings[NElements];

public:
    constexpr uint16_t cacheId() const { return Size ^ NElements; }
    constexpr size_t maxStringSize() const { return Size - BufStringOverHead; }
    constexpr size_t size() const { return m_size; }
    constexpr size_t capacity() const { return NElements - m_size; }

    BufStringCacheHandle put(const char *str, size_t length)
    {
        BufStringCacheHandle hnd{};

        if (length > maxStringSize())
        {
            return hnd;
        }

        hnd.cacheId = cacheId();
        hnd.index = NElements; // Invalid;
        hnd.maxSize = Size;

        // check duplicates
        for (size_t i = 0; i < m_size; i++)
        {
            if (m_strings[i] == str)
            {
                hnd.index = i;
                hnd.base = m_strings[i].base();
                return hnd;
            }
        }

#ifdef QT_DEBUG
        assert(m_size < NElements);
#endif
        if (m_size < NElements)
        {
            m_strings[m_size].setString(str, length);
            hnd.index = m_size;
            hnd.base = m_strings[m_size].base();
            m_size++;
        }

        return hnd;
    }

    constexpr const BufString<Size> &get(BufStringCacheHandle hnd) const
    {
#ifdef QT_DEBUG
        assert(hnd.cacheId == cacheId());
        assert(hnd.index < NElements);
#endif
        if (hnd.index < NElements)
        {
            return m_strings[hnd.index];
        }

        return m_strings[0];
    }
};

#endif // BUF_STRING_H
