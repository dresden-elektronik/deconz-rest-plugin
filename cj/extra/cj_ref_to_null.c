/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

/** Tests if JSON token reference is null.
 *
 * \param ctx the CJ context.
 * \param ref the token reference of the value.
 *
 * \return 1 on success the token is 'null'
 *         0 on failure
 */
int cj_ref_to_null(cj_ctx *ctx, cj_token_ref ref)
{
    cj_token *tok;
    const char *p;

    if (ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];
        if (tok->type != CJ_TOKEN_PRIMITIVE)
            return 0;

        p = (const char*)&ctx->buf[tok->pos];
        if (tok->len == 4 && p[0] == 'n' && p[1] == 'u' && p[2] == 'l' && p[3] == 'l')
            return 1;
    }

    return 0;
}
