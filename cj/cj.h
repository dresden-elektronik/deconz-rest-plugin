/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef _CJSON_H
#define _CJSON_H

#ifdef CJ_USE_64BIT_SIZE_T
  #ifdef __LP64__
    typedef unsigned long cj_size;
  #endif

  #ifndef __LP64__ /* needs at least C99 and C++11 */
    typedef unsigned long long cj_size;
  #endif
#else /* 32-bit (default) */
  #ifdef __LP64__
    typedef unsigned cj_size;
  #endif

  #ifndef __LP64__
      typedef unsigned long cj_size;
  #endif
#endif

#define CJ_INVALID_TOKEN_INDEX ((cj_size)~0)

typedef cj_size cj_token_ref;

typedef enum cj_status
{
    CJ_OK                     = 0,
    CJ_ERROR                  = 1,
    CJ_INVALID_UTF8           = 2,
    CJ_PARSE_TOKENS_EXHAUSTED = 3,
    CJ_PARSE_PARENT_CLOSING   = 4,
    CJ_PARSE_INVALID_TOKEN    = 5,
    CJ_PARSE_INVALID_OBJECT   = 6,
    CJ_PARSE_MULTI_TOP_THINGS = 7
} cj_status;

typedef enum cj_token_type
{
    CJ_TOKEN_INVALID    = 'i',
    CJ_TOKEN_STRING     = 'S',
    CJ_TOKEN_PRIMITIVE  = 'P',
    CJ_TOKEN_ARRAY_BEG  = '[',
    CJ_TOKEN_ARRAY_END  = ']',
    CJ_TOKEN_OBJECT_BEG = '{',
    CJ_TOKEN_OBJECT_END = '}',
    CJ_TOKEN_ITEM_SEP   = ',',
    CJ_TOKEN_NAME_SEP   = ':'
} cj_token_type;

typedef struct cj_token
{
    cj_token_type type;
    cj_size pos; /* position in JSON string */
    cj_size len; /* length of the token in bytes */
    cj_token_ref parent;
} cj_token;

typedef struct cj_ctx
{
    /* input JSON */
    const unsigned char *buf;
    cj_size pos;
    cj_size size;

    /* parse */
    cj_token *tokens;
    cj_size tokens_pos;
    cj_size tokens_size;

    cj_status status;
} cj_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the parse context.
 *
 * \param ctx the CJ context.
 * \param json a JSON string.
 * \param len strlen of the JSON string.
 * \param tokens an array of tokens which can be filled by the parser.
 * \param tokens_size count of tokens.
 */
void cj_parse_init(cj_ctx *ctx, const char *json, cj_size len, cj_token *tokens, cj_size tokens_size);

/** Parses the formerly initialzed context.
 *
 * \param ctx the CJ context.
 * \return ctx->status as result
 */
void cj_parse(cj_ctx *ctx);

/** Get the token reference of a key in an object.
 *
 * \param ctx the CJ context.
 * \param obj the token reference of the parent object.
 * \param key the key to find.
 *
 * \return a valid token reference on success
 *         CJ_INVALID_TOKEN_INDEX on failure
 */
cj_token_ref cj_value_ref(cj_ctx *ctx, cj_token_ref obj, const char *key);

/** Copy the value of an object key as string into a buffer.
 *
 * \param ctx the CJ context.
 * \param buf destination buffer.
 * \param size size of the destination buffer.
 * \param obj the token reference of the parent object.
 * \param key the key of the value.
 *
 * \return 1 on success
 *         0 on failure
 */
int cj_copy_value(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref obj, const char *key);

/** Copy the value of a token as string into a buffer.
 *
 * \param ctx the CJ context.
 * \param buf destination buffer.
 * \param size size of the destination buffer.
 * \param ref the token reference of the value.
 *
 * \return 1 on success
 *         0 on failure
 */
int cj_copy_ref(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref ref);

#ifdef __cplusplus
}
#endif

#endif /* _CJSON_H */
