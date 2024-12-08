/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "cj.h"

#ifdef NDEBUG
  #define CJ_ASSERT(c) ((void)0)
#else
  #if _MSC_VER
    #define CJ_ASSERT(c) if (!(c)) __debugbreak()
  #elif __GNUC__
    #define CJ_ASSERT(c) if (!(c)) __builtin_trap()
  #else
    #define CJ_ASSERT(c) ((void)0)
  #endif
#endif /* NDEBUG */

/* Convert utf-8 to unicode code point.

   Returns >0 as number of bytes in utf8 character, and 'codepoint' set
   or 0 for invalid utf8, codepoint set to 0.

 */
static int cj_utf8_to_codepoint(const unsigned char *str, unsigned long len, unsigned long *codepoint)
{
    int result;
    unsigned long cp;
    unsigned bytes;

    if (str && len != 0)
        cp = (unsigned)*str & 0xFF;
    else
        goto invalid;

    for (bytes = 0; cp & 0x80; bytes++)
        cp = (cp & 0x7F) << 1;

    if (bytes == 0) /* ASCII */
    {
        *codepoint = cp;
        return 1;
    }

    if (bytes > 4 || bytes > len)
        goto invalid;

    result = (int)bytes;

    /* 110xxxxx 10xxxxxx */
    /* 1110xxxx 10xxxxxx 10xxxxxx */
    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    cp >>= bytes;
    bytes--;
    str++;

    for (;bytes; bytes--, str++)
    {
        if (((unsigned)*str & 0xC0) != 0x80) /* must start with 10xxxxxx */
            goto invalid;

        cp <<= 6;
        cp |= (unsigned)*str & 0x3F;
    }

    if      (result == 2 &&  cp < 0x80)  goto invalid;
    else if (result == 3 &&  cp < 0x800) goto invalid;
    else if (result == 4 && (cp < 0x10000 || cp > 0x10FFFF)) goto invalid;

    *codepoint = cp;
    return result;

invalid:
    *codepoint = 0;
    return 0;
}

static cj_status cj_is_valid_utf8(const unsigned char *str, unsigned long len)
{
    int ch_count;
    unsigned long codepoint;

    do /* test valid utf8 codepoints */
    {
        ch_count = cj_utf8_to_codepoint(str, len, &codepoint);
        if (ch_count > 0)
        {
            str += ch_count;
            len -= (unsigned)ch_count;
        }
        else
        {
            return CJ_INVALID_UTF8;
        }
    } while (ch_count > 0 && len > 0);

    return CJ_OK;
}

static unsigned long cj_eat_white_space(const unsigned char *str, unsigned long len)
{
    unsigned long result;

    result = 0;

    for (; str[result] && result < len; )
    {
        switch(str[result])
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            result++;
            break;
        case '\0':
        default:
            goto out;
        }
    }

out:
    return result;
}

static int cj_is_primitive_char(unsigned char c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '+' || c == '.' || c == '-') ? 1 : 0;
}

static cj_token_ref cj_alloc_token(cj_ctx *ctx, cj_token **tok)
{
    cj_token_ref result;

    result = CJ_INVALID_TOKEN_INDEX;

    if (ctx->tokens_pos < ctx->tokens_size)
    {
        CJ_ASSERT((cj_token_ref)ctx->tokens_pos >= 0);
        result = (cj_token_ref)ctx->tokens_pos;
        ctx->tokens_pos++;

        *tok = &ctx->tokens[result];
        (*tok)->type = CJ_TOKEN_INVALID;
        (*tok)->parent = CJ_INVALID_TOKEN_INDEX;
    }
    else
    {
        *tok = 0;
    }

    return result;
}

enum cj_primitive_state
{
    CJ_PRIM_STATE_INIT,
    CJ_PRIM_STATE_NULL_N,
    CJ_PRIM_STATE_NULL_U,
    CJ_PRIM_STATE_NULL_L1,
    CJ_PRIM_STATE_TRUE_T,
    CJ_PRIM_STATE_TRUE_R,
    CJ_PRIM_STATE_TRUE_U,
    CJ_PRIM_STATE_FALSE_F,
    CJ_PRIM_STATE_FALSE_A,
    CJ_PRIM_STATE_FALSE_L,
    CJ_PRIM_STATE_FALSE_S,
    CJ_PRIM_STATE_FALSE_E,
    CJ_PRIM_STATE_NUMBER_SIGN,
    CJ_PRIM_STATE_NUMBER_INITIAL_ZERO,
    CJ_PRIM_STATE_NUMBER_DIGIT,
    CJ_PRIM_STATE_NUMBER_DOT,
    CJ_PRIM_STATE_NUMBER_FRACT_DIGIT,
    CJ_PRIM_STATE_NUMBER_EXP_E,
    CJ_PRIM_STATE_NUMBER_EXP_SIGN,
    CJ_PRIM_STATE_NUMBER_EXP_DIGIT,
    CJ_PRIM_STATE_FINISH
};

static cj_size cj_next_token(const unsigned char *str, cj_size len, cj_size pos, cj_token *tok)
{
    int esc;
    unsigned char ch;
    enum cj_primitive_state prim_state;
    enum cj_primitive_state prim_state_next;

    CJ_ASSERT(pos <= len);
    pos += cj_eat_white_space(&str[pos], len - pos);

    tok->type = CJ_TOKEN_INVALID;
    tok->pos = pos;
    tok->len = 0;
    esc = 0;

    for (;pos < len; pos++)
    {
        ch = str[pos];

        if (tok->type == CJ_TOKEN_INVALID)
        {
            switch (ch)
            {
                case '{': tok->type = CJ_TOKEN_OBJECT_BEG; tok->len = 1; return ++pos;
                case '}': tok->type = CJ_TOKEN_OBJECT_END; tok->len = 1; return ++pos;
                case '[': tok->type = CJ_TOKEN_ARRAY_BEG;  tok->len = 1; return ++pos;
                case ']': tok->type = CJ_TOKEN_ARRAY_END;  tok->len = 1; return ++pos;
                case ',': tok->type = CJ_TOKEN_ITEM_SEP;   tok->len = 1; return ++pos;
                case ':': tok->type = CJ_TOKEN_NAME_SEP;   tok->len = 1; return ++pos;
                case '\"': tok->type = CJ_TOKEN_STRING; tok->pos++; break;
                default:
                    break;
            }

            if (tok->type != CJ_TOKEN_INVALID)
                continue;

            prim_state = CJ_PRIM_STATE_INIT;
            tok->type = CJ_TOKEN_PRIMITIVE;
            tok->len = 0;
            pos--;
        }
        else if (tok->type == CJ_TOKEN_STRING)
        {
            if (esc == 0)
            {
                if (ch == '\"') /* end of string */
                    return ++pos;

                if (ch < 0x20) /* needs to be escaped */
                {
                    tok->type = CJ_TOKEN_INVALID;
                    return pos;
                }

                if (ch == '\\')
                    esc = 1;
            }
            else if (esc == 1)
            {
                switch (ch)
                {
                case '"':
                case 'n':
                case '\\':
                case '/':
                case 't':
                case 'r':
                case 'b':
                case 'f':
                    esc = 0;
                    break;

                case 'u':
                    esc = 2;
                    break;

                default:
                    tok->type = CJ_TOKEN_INVALID;
                    return pos;
                }
            }
            else if (esc >= 2)
            {
                if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || (ch >= '0' && ch <= '9'))
                {
                    esc++;
                    if (esc == 6)
                        esc = 0;
                }
                else
                {
                    tok->type = CJ_TOKEN_INVALID;
                    return pos;
                }
            }

            tok->len++;
        }
        else if (tok->type == CJ_TOKEN_PRIMITIVE)
        {
            prim_state_next = CJ_PRIM_STATE_INIT;

            if (ch == '\0') /* zero terrminator not allowed */
                goto invalid_primitive;

            switch (prim_state)
            {
            case CJ_PRIM_STATE_INIT:
            {
                switch (ch)
                {
                case 'n': prim_state_next = CJ_PRIM_STATE_NULL_N; break;
                case 't': prim_state_next = CJ_PRIM_STATE_TRUE_T; break;
                case 'f': prim_state_next = CJ_PRIM_STATE_FALSE_F; break;
                case '-': prim_state_next = CJ_PRIM_STATE_NUMBER_SIGN; break;
                case '0': prim_state_next = CJ_PRIM_STATE_NUMBER_INITIAL_ZERO; break;
                default:
                    if (ch >= '1' && ch <= '9')
                        prim_state_next = CJ_PRIM_STATE_NUMBER_DIGIT;
                    break;
                }
            }
                break;

            case CJ_PRIM_STATE_NUMBER_SIGN:
                if      (ch == '0')              prim_state_next = CJ_PRIM_STATE_NUMBER_INITIAL_ZERO;
                else if (ch >= '1' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_DIGIT;
                break;

            case CJ_PRIM_STATE_NUMBER_DIGIT:
                if      (ch >= '0' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_DIGIT;
                else if (ch == '.')              prim_state_next = CJ_PRIM_STATE_NUMBER_DOT;
                else if (ch == 'e' || ch == 'E') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_E;
                break;

            case CJ_PRIM_STATE_NUMBER_INITIAL_ZERO:
                if      (ch == '.')              prim_state_next = CJ_PRIM_STATE_NUMBER_DOT;
                else if (ch == 'e' || ch == 'E') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_E;
                break;


            case CJ_PRIM_STATE_NUMBER_DOT:
                if      (ch >= '0' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_FRACT_DIGIT;
                break;

            case CJ_PRIM_STATE_NUMBER_FRACT_DIGIT:
                if      (ch >= '0' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_FRACT_DIGIT;
                else if (ch == 'e' || ch == 'E') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_E;
                break;

            case CJ_PRIM_STATE_NUMBER_EXP_E:
                if      (ch >= '0' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_DIGIT;
                else if (ch == '+' || ch == '-') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_SIGN;
                break;

            case CJ_PRIM_STATE_NUMBER_EXP_SIGN:
                if      (ch >= '0' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_DIGIT;
                break;

            case CJ_PRIM_STATE_NUMBER_EXP_DIGIT:
                if      (ch >= '0' && ch <= '9') prim_state_next = CJ_PRIM_STATE_NUMBER_EXP_DIGIT;
                break;

            case CJ_PRIM_STATE_NULL_N:  if (ch == 'u') prim_state_next = CJ_PRIM_STATE_NULL_U; break;
            case CJ_PRIM_STATE_NULL_U:  if (ch == 'l') prim_state_next = CJ_PRIM_STATE_NULL_L1; break;
            case CJ_PRIM_STATE_NULL_L1: if (ch == 'l') prim_state_next = CJ_PRIM_STATE_FINISH; break;

            case CJ_PRIM_STATE_TRUE_T:  if (ch == 'r') prim_state_next = CJ_PRIM_STATE_TRUE_R; break;
            case CJ_PRIM_STATE_TRUE_R:  if (ch == 'u') prim_state_next = CJ_PRIM_STATE_TRUE_U; break;
            case CJ_PRIM_STATE_TRUE_U:  if (ch == 'e') prim_state_next = CJ_PRIM_STATE_FINISH; break;

            case CJ_PRIM_STATE_FALSE_F:  if (ch == 'a') prim_state_next = CJ_PRIM_STATE_FALSE_A; break;
            case CJ_PRIM_STATE_FALSE_A:  if (ch == 'l') prim_state_next = CJ_PRIM_STATE_FALSE_L; break;
            case CJ_PRIM_STATE_FALSE_L:  if (ch == 's') prim_state_next = CJ_PRIM_STATE_FALSE_S; break;
            case CJ_PRIM_STATE_FALSE_S:  if (ch == 'e') prim_state_next = CJ_PRIM_STATE_FINISH; break;

            default:
                break;
            }

            if (prim_state_next == CJ_PRIM_STATE_INIT)
            {
                switch (prim_state) /* test valid end states */
                {
                case CJ_PRIM_STATE_NUMBER_EXP_DIGIT:
                case CJ_PRIM_STATE_NUMBER_FRACT_DIGIT:
                case CJ_PRIM_STATE_NUMBER_INITIAL_ZERO:
                case CJ_PRIM_STATE_NUMBER_DIGIT:
                    return pos;
                    break;

                default:
                    goto invalid_primitive;
                }
            }

            tok->len++;
            if (prim_state_next == CJ_PRIM_STATE_FINISH)
                return ++pos;

            prim_state = prim_state_next;
            continue;

invalid_primitive:
            tok->type = CJ_TOKEN_INVALID;
            break;
        }
        else
        {
            break;
        }
    }

    /* string end is detected above and returns */
    if (tok->type == CJ_TOKEN_STRING)
        tok->type = CJ_TOKEN_INVALID;

    return pos;
}

static void cj_trim_trailing_whitespace(cj_ctx *ctx)
{
    for (;ctx->size > 0;)
    {
        switch(ctx->buf[ctx->size - 1])
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            ctx->size--;
            break;
        default:
            return;
        }
    }
}

void cj_parse_init(cj_ctx *ctx, const char *json, cj_size len,
                   cj_token *tokens, cj_size tokens_size)
{
    if (!ctx)
        return;

    ctx->buf = (const unsigned char*)json;
    ctx->pos = 0;
    ctx->size = len;

    ctx->tokens = tokens;
    ctx->tokens_pos = 0;
    ctx->tokens_size = tokens_size;

    if (!json || len == 0 || !tokens || tokens_size < 8)
        ctx->status = CJ_ERROR;
    else
        ctx->status = CJ_OK;
}

void cj_parse(cj_ctx *ctx)
{
    int obj_depth;
    int arr_depth;
    cj_size pos;
    unsigned ncommas;
    unsigned ncolons;
    cj_size nstructures;
    cj_token_ref tok_index;
    cj_token_ref tok_parent;
    cj_token *tok;
    const unsigned char *p;

    if (!ctx || ctx->status != CJ_OK)
        return;

    ctx->status = cj_is_valid_utf8(ctx->buf, ctx->size);
    if (ctx->status != CJ_OK)
        return;

    cj_trim_trailing_whitespace(ctx);

    /* to track valid objects */
    ncolons = 0;
    ncommas = 0;

    nstructures = 0;
    obj_depth = 0;
    arr_depth = 0;
    pos = 0;
    p = ctx->buf;
    tok_parent = CJ_INVALID_TOKEN_INDEX;

    do
    {
        tok_index = cj_alloc_token(ctx, &tok);
        if (tok_index == CJ_INVALID_TOKEN_INDEX || tok == 0)
        {
            ctx->status = CJ_PARSE_TOKENS_EXHAUSTED;
            break;
        }

        pos = cj_next_token(p, ctx->size, pos, tok);

        if (tok->type == CJ_TOKEN_INVALID)
        {
            ctx->status = CJ_PARSE_INVALID_TOKEN;
            break;
        }

        tok->parent = tok_parent;
        CJ_ASSERT(tok->parent == CJ_INVALID_TOKEN_INDEX || tok->parent < ctx->tokens_pos);

        if (tok->type == CJ_TOKEN_OBJECT_BEG)
        {
            if (ctx->tokens_pos > 1 && arr_depth == 0 && obj_depth == 0)
            {
                ctx->status = CJ_PARSE_MULTI_TOP_THINGS;
                break;
            }
            nstructures++;
            obj_depth++;
            tok_parent = tok_index;
            CJ_ASSERT(tok_parent < ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_ARRAY_BEG)
        {
            if (ctx->tokens_pos > 1 && arr_depth == 0 && obj_depth == 0)
            {
                ctx->status = CJ_PARSE_MULTI_TOP_THINGS;
                break;
            }
            nstructures++;
            arr_depth++;
            tok_parent = tok_index;
            CJ_ASSERT(tok_parent < ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_OBJECT_END)
        {
            obj_depth--;
            if (obj_depth < 0 || tok_index < 1)
            {
                ctx->status = CJ_PARSE_PARENT_CLOSING;
                break;
            }

            CJ_ASSERT(tok->parent < ctx->tokens_pos);
            tok_parent = ctx->tokens[tok->parent].parent;
            tok->parent = tok_parent;
            CJ_ASSERT(tok->parent == CJ_INVALID_TOKEN_INDEX || tok_parent < ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_ARRAY_END)
        {
            arr_depth--;
            if (arr_depth < 0 || tok_index < 1)
            {
                ctx->status = CJ_PARSE_PARENT_CLOSING;
                break;
            }
            CJ_ASSERT(tok->parent < ctx->tokens_pos);
            tok_parent = ctx->tokens[tok->parent].parent;
            tok->parent = tok_parent;
            CJ_ASSERT(tok->parent == CJ_INVALID_TOKEN_INDEX || tok_parent < ctx->tokens_pos);
        }

#if 0
        if (tok->type == CJ_TOKEN_STRING || tok->type == CJ_TOKEN_PRIMITIVE)
        {
            printf("JSON TOKEN[%d] (%c) pos: %u, len: %u, parent: %d, obj_depth: %d, arr_depth: %d, %.*s\n",
                tok_index, (char)tok->type, tok->pos, tok->len, tok->parent, obj_depth, arr_depth,
                tok->len, &ctx->buf[tok->pos]);
        }
        else
        {
            printf("JSON TOKEN[%d] (%c) pos: %u, len: %u, parent: %d, obj_depth: %d, arr_depth: %d\n",
                tok_index, (char)tok->type, tok->pos, tok->len, tok->parent, obj_depth, arr_depth);
        }
#endif

        if (tok->type == CJ_TOKEN_NAME_SEP)
        {
            if (tok_index < 2 || tok->parent == CJ_INVALID_TOKEN_INDEX || tok[-1].type != CJ_TOKEN_STRING)
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (ctx->tokens[tok->parent].type != CJ_TOKEN_OBJECT_BEG)
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }
            ncolons++;
        }

        if (tok->type == CJ_TOKEN_ITEM_SEP)
        {
            if (tok_index < 2 || tok->parent == CJ_INVALID_TOKEN_INDEX ||
                tok[-1].type == CJ_TOKEN_OBJECT_BEG || tok[-1].type == CJ_TOKEN_ARRAY_BEG)
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (ctx->tokens[tok->parent].type == CJ_TOKEN_OBJECT_BEG)
            {
                ncommas++;
            }
        }

        if (tok_index >= 1)
        {
            if (tok[-1].type == CJ_TOKEN_NAME_SEP &&
                !(tok->type == CJ_TOKEN_STRING ||
                  tok->type == CJ_TOKEN_PRIMITIVE ||
                  tok->type == CJ_TOKEN_OBJECT_BEG ||
                  tok->type == CJ_TOKEN_ARRAY_BEG))
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (tok[-1].type == CJ_TOKEN_ITEM_SEP &&
                !(tok->type == CJ_TOKEN_STRING ||
                  tok->type == CJ_TOKEN_PRIMITIVE ||
                  tok->type == CJ_TOKEN_OBJECT_BEG ||
                  tok->type == CJ_TOKEN_ARRAY_BEG))
            {
                pos = tok[-1].pos;
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (tok[-1].type == CJ_TOKEN_PRIMITIVE &&
                !(tok->type == CJ_TOKEN_ITEM_SEP ||
                  tok->type == CJ_TOKEN_OBJECT_END ||
                  tok->type == CJ_TOKEN_ARRAY_END))
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (tok[-1].type == CJ_TOKEN_STRING &&
                !(tok->type == CJ_TOKEN_ITEM_SEP ||
                  tok->type == CJ_TOKEN_NAME_SEP ||
                  tok->type == CJ_TOKEN_OBJECT_END ||
                  tok->type == CJ_TOKEN_ARRAY_END))
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }
        }
    }
    while (pos < ctx->size);

    ctx->pos = pos;

    if (ctx->status == CJ_OK)
    {
        if (obj_depth != 0 || arr_depth != 0)
        {
            ctx->status = CJ_PARSE_PARENT_CLOSING;
        }
        else if (nstructures > 0)
        {
            tok = &ctx->tokens[ctx->tokens_pos - 1];
            if (tok->type != CJ_TOKEN_OBJECT_END && tok->type != CJ_TOKEN_ARRAY_END)
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
            }
            else if (ncommas && ncommas >= ncolons)
            {
                ctx->status = CJ_PARSE_INVALID_OBJECT;
            }
        }
    }
}

cj_token_ref cj_value_ref(cj_ctx *ctx, cj_token_ref obj, const char *key)
{
    cj_token_ref i;
    unsigned k;
    unsigned len;
    cj_token_ref result;
    cj_token *tok;

    result = CJ_INVALID_TOKEN_INDEX;

    if (!ctx || ctx->status != CJ_OK || !key)
        return result;

    if (obj < 0 || obj >= (cj_token_ref)ctx->tokens_pos)
        return result;

    for (len = 0; key[len]; len++)
    {}

    for (i = obj + 1; i < (cj_token_ref)ctx->tokens_pos; i++)
    {
        tok = &ctx->tokens[i];
        if (tok->type != CJ_TOKEN_NAME_SEP)
            continue;

        tok = &tok[-1];

        if (tok->parent != obj)
            continue;

        if (tok->len != len)
            continue;

        for (k = 0; k < len; k++)
        {
            if (ctx->buf[tok->pos + k] != (unsigned char)key[k])
                break;
        }

        if (k == len)
        {
            result = i + 1;
            break;
        }
    }

    return result;
}

int cj_copy_value(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref obj, const char *key)
{
    cj_size i;
    cj_token_ref ref;
    cj_token *tok;
    unsigned char *out;

    out = (unsigned char*)buf;
    out[0] = '\0';
    ref = cj_value_ref(ctx, obj, key);

    if (ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];
        if (tok->len < size)
        {
            for (i = 0; i < tok->len; i++)
                out[i] = ctx->buf[tok->pos + i];
            out[tok->len] = '\0';
            return 1;
        }
    }

    return 0;
}

int cj_copy_ref(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref ref)
{
    unsigned i;
    cj_token *tok;
    unsigned char *out;

    out = (unsigned char*)buf;
    out[0] = '\0';

    if (ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];
        if (tok->len < size)
        {
            for (i = 0; i < tok->len; i++)
                out[i] = ctx->buf[tok->pos + i];
            out[tok->len] = '\0';
            return 1;
        }
    }

    return 0;
}
