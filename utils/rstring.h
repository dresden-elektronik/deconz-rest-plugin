/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef BufString_H
#define BufString_H

#include <QString>
#include <cstddef>
#include <cstring>

class QString;

constexpr size_t BufStringOverHead = 2; // length + null termintor

template <size_t Size>
class BufString
{
    char buf[Size] = { };

    static_assert (Size <= (255 + BufStringOverHead), "Size too large");

public:
    BufString()
    { clear(); }

    BufString(const char *str) :
        BufString()
    { setString(str, strlen(str)); }

    bool setString(const char *str)
    {
        return setString(str, strlen(str));
    }

    bool setString(const char *str, const size_t len)
    {
        if (len > maxSize())
        {
            return false;
        }

        buf[0] = len;
        if (len)
        {
            memcpy(&buf[1], str, len);
        }
        buf[1 + len] = '\0';
        return true;
    }

    void clear()
    {
        buf[0] = 0;
        buf[1] = '\0';
    }

    const char *c_str() const { return &buf[1]; };
    bool empty() const { return size() == 0; }
    size_t size() const { return buf[0]; }
    constexpr size_t maxSize() const { return Size - BufStringOverHead; }
    size_t capacity() const { return maxSize() - size(); }

    operator QString () const { return QString::fromUtf8(c_str(), int(size())); }
    operator QLatin1String () const { return QLatin1String(c_str(), int(size())); }

    bool startsWith(const QLatin1String &str) const
    {
        if (str.size() <= int(size()))
        {
            return QLatin1String(c_str(), int(size())).startsWith(str);
        }
        return false;
    }

    bool startsWith(const QString &str) const
    {
        if (str.size() <= size())
        {
            const QString q = *this;
            return q.startsWith(str);
        }
        return false;
    }
};

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
    return !rhs.isEmpty() && strcmp(lhs.c_str(), rhs.data()) == 0;
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


#endif // BufString_H
