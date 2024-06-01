/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

/** Converts a base 10 number string to signed long.
 *
 * This is a naive potentially slow function (no use of libc).
 *
 * Depending on sizeof(long) 4/8 the valid numeric range is:
 *
 *   32-bit: -2147483648 ... 2147483647
 *   64-bit: -9223372036854775808 ... 9223372036854775807
 *
 * The err variable is a bitmap:
 *
 *   0x01 invalid input
 *   0x02 range overflow
 *   0x04 range underflow
 *
 * \param s pointer to string, doesn't have to be '\0' terminated.
 * \param len length of s ala strlen(s).
 * \param endp pointer which will be set to first non 0-9 character (must NOT be NULL).
 * \param err pointer to error variable (must NOT be NULL).
 *
 * \return If the conversion is successful the number is returned and err set to 0.
 *         On failure err has a non zero value.
 */
long cj_parse_long(const char *s, cj_size len, const char **endp, int *err)
{
    int i;
    int e;
    long ch;
    unsigned long max;
    unsigned long result;

    e = 0;
    ch = 0;
    result = 0;

    max = ~0;
    max >>= 1;

    if (len == 0)
    {
        *err = 1;
        *endp = s;
        return 0;
    }

    i = *s == '-' ? 1 : 0;

    for (; i < len; i++)
    {
        ch = s[i];
        if (ch < '0' || ch > '9')
            break;

        ch = ch - '0';
        e |= (result * 10 + ch < result) ? 2 : 0; /* overflow */
        result *= 10;

        result += ch;
    }

    if      (i == 1 && *s == '-') e |= 1;
    else if (i == 0)              e |= 1;

    if (result > max)
    {
        if      (*s != '-')           e |= 2; /* overflow */
        else if (result > max + 1)    e |= 4; /* underflow */
    }

    *endp = &s[i];
    *err = e;

    if (*s == '-')
        return -(long)result;

    return (long)result;
}

/** Converts a JSON token reference to signed long.
 *
 * This is NOT using libc but a custom implementation.
 *
 * Note depending on the architecture long is either 32-bit
 * or 64-bit with sizeof(long) either 4 or 8. The numeric
 * range therefore varies by this.
 *
 * If the conversion ends up in overflow or underflow,
 * 0 is returned with an error bitmap in result:
 *
 *   0x01 invalid input
 *   0x02 range overflow
 *   0x04 range underflow
 *
 * \param ctx the CJ context.
 * \param result pointer to result long variable.
 * \param ref the token reference of the value.
 *
 * \return 1 on success
 *         0 on failure
 */
int cj_ref_to_long(cj_ctx *ctx, long *result, cj_token_ref ref)
{
    int err;
    long conv;
    cj_token *tok;
    const char *p;
    const char *endptr;

    if (result && ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];
        if (tok->type != CJ_TOKEN_PRIMITIVE || tok->len == 0)
            return 0;

        p = (const char*)&ctx->buf[tok->pos];

        conv = cj_parse_long(p, tok->len, &endptr, &err);

        if (err)
        {
            *result = err;
            return 0;
        }

        if (endptr == p + tok->len)
        {
            *result = conv;
            return 1;
        }
    }

    return 0;
}
