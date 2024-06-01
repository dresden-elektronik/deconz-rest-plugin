/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

enum cj_unicode_state
{
    CJ_UC_STATE_NONE,
    CJ_UC_STATE_ESC,
    CJ_UC_STATE_NUM0,
    CJ_UC_STATE_NUM1,
    CJ_UC_STATE_NUM2,
    CJ_UC_STATE_NUM3,
    CJ_UC_STATE_CHK_NUM
};

/* The buffer must be of size 5, the string gets zero terminated.
*/
int cj_unicode_to_utf8(unsigned long codepoint, unsigned char *buf, cj_size size)
{
    if (codepoint <= 0x7F && size > 1)
    {
        /* 1-byte ASCII */
        buf[0] = (char)codepoint;
        buf[1] = '\0';
        return 1;
    }

    if (codepoint > 0x10FFFF)
        codepoint = 0xFFFD; /* codepoint replacement character */

    if (codepoint >= 0x80 && codepoint <= 0x7FF && size > 2) /*  110 prefix 2-byte char */
    {
        buf[1] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[0] = 0xC0 | (codepoint & 0x1F);
        buf[2] = '\0';
        return 2;
    }
    else if (codepoint >= 0x800 && codepoint <= 0xFFFF && size > 3) /*  1110 prefix 3-byte char */
    {
        /* 1110xxxx 10xxxxxx 10xxxxxx */
        buf[2] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[1] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[0] = 0xE0 | (codepoint & 0x0F);
        buf[3] = '\0';
        return 3;
    }
    else if (codepoint >= 0x10000 && codepoint <= 0x10FFFF && size > 4) /*  11110 prefix 4-byte char */
    {
        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        buf[3] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[2] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[1] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[0] = 0xF0 | (codepoint & 0x07);
        buf[4] = '\0';
        return 4;
    }

    return 0;
}

/** Copy the value of a token as UTF-8 string into a buffer.
 *
 * Values that are escaped as UTF-16 \uXXXX or surrogate pair
 * \uXXXX\uXXXX will be converted to UTF-8 representation.
 *
 * Surrogate paires are verified to be complete.
 *
 * \param ctx the CJ context.
 * \param buf destination buffer.
 * \param size size of the destination buffer.
 * \param ref the token reference of the value.
 *
 * \return 1 on success
 *         0 on failure
 */
int cj_copy_ref_utf8(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref ref)
{
    int n;
    unsigned i;
    cj_token *tok;
    const unsigned char *ch;
    unsigned char *wr;
    const unsigned char *wr_end;
    enum cj_unicode_state state;
    int need_low_surrogate;
    unsigned long c;
    unsigned long num0;
    unsigned long high_surrogate;

    if (size)
        buf[0] = '\0';

    if (size > 1 && ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];

        if (tok->type != CJ_TOKEN_STRING)
            return cj_copy_ref(ctx, buf, size, ref);

        num0 = 0;
        high_surrogate = 0;
        need_low_surrogate = 0;
        ch = &ctx->buf[tok->pos];
        state = CJ_UC_STATE_NONE;
        wr = (unsigned char*)buf;
        wr_end = wr + size;

        for (i = 0; i < tok->len; i++, ch++)
        {
            if ((wr_end - wr) == 1) /* not enough space to copy more and '\0' */
                goto err;

            c = *ch;

            switch (state)
            {
            case CJ_UC_STATE_NONE:
                if (c == '\\') state = CJ_UC_STATE_ESC;
                else           *wr++ = c;
                break;

            case CJ_UC_STATE_ESC:
                if (c == 'u')
                    state = CJ_UC_STATE_NUM0;
                else /* not an unicode escape */
                {
                    switch (c)
                    {
                    case 'n':  *wr++ = '\n'; break;
                    case '"':  *wr++ = '"';  break;
                    case '\\': *wr++ = '\\'; break;
                    case 't':  *wr++ = '\t'; break;
                    case '/':  *wr++ = '/';  break;
                    case 'r':  *wr++ = 'r';  break;
                    case 'b':  *wr++ = 'b';  break;
                    case 'f':  *wr++ = 'f';  break;
                    default:
                        goto err; /* unsupported escaped character */
                    }
                    state = CJ_UC_STATE_NONE;
                }
                break;

            case CJ_UC_STATE_NUM0:
            case CJ_UC_STATE_NUM1:
            case CJ_UC_STATE_NUM2:
            case CJ_UC_STATE_NUM3: /* fall through */
                if      (c >= 'a' && c <= 'f') c = c - 'a' + 10; /* 10..15 */
                else if (c >= 'A' && c <= 'F') c = c - 'A' + 10; /* 10..15 */
                else if (c >= '0' && c <= '9') c = c - '0';      /*  0..9  */
                else goto err;

                num0 <<= 4;
                num0 |= c;
                state = (enum cj_unicode_state)((int)state + 1);
                /* if we are the last char in sequence fall through! */
                if (state != CJ_UC_STATE_CHK_NUM)
                    break;

            case CJ_UC_STATE_CHK_NUM:
                {
                    if (need_low_surrogate == 0)
                    {
                        if (num0 >= 0xDC00 && num0 <= 0xDFFF)
                            goto err; /* isolated low-surrogate is invalid */

                        if (num0 >= 0xD800 && num0 <= 0xDBFF)
                        {
                            need_low_surrogate = 1;
                            high_surrogate = (num0 - 0xD800) * 0x400;

                            if ((tok->len - i) < 6 || ch[1] != '\\' || ch[2] != 'u')
                                goto err; /* low-surrogate must be next */
                        }
                    }
                    else
                    {
                        if (num0 < 0xDC00 || num0 > 0xDFFF)
                            goto err;

                        num0 -= 0xDC00;
                        num0 = high_surrogate + num0 + 0x10000;
                        need_low_surrogate = 0;
                    }

                    if (need_low_surrogate == 0)
                    {
                        n = cj_unicode_to_utf8(num0, wr, wr_end - wr);
                        if (n == 0)
                            goto err;
                        wr += n;
                    }

                    num0 = 0;
                    state = CJ_UC_STATE_NONE;
                }
                break;

            default:
                goto err; /* should never happen */
            }
        }
    }
    else
    {
        goto err;
    }

    if (wr < wr_end)
        *wr = '\0';

    if (state != CJ_UC_STATE_NONE)
        goto err;

    return 1;

err:
    if (size)
        buf[0] = '\0';
    return 0;
}
