/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

/** Converts a JSON token reference to boolean.
 *
 * \param ctx the CJ context.
 * \param result pointer: set to 1 for true and 0 for false.
 * \param ref the token reference of the value.
 *
 * \return 1 on success
 *         0 on failure
 */
int cj_ref_to_boolean(cj_ctx *ctx, int *result, cj_token_ref ref)
{
    cj_token *tok;
    const char *p;

    if (result && ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];
        if (tok->type != CJ_TOKEN_PRIMITIVE)
            return 0;

        p = (const char*)&ctx->buf[tok->pos];
        if (tok->len == 4 && p[0] == 't' && p[1] == 'r' && p[2] == 'u' && p[3] == 'e')
        {
            *result = 1;
            return 1;
        }
        else if (tok->len == 5 && p[0] == 'f' && p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e')
        {
            *result = 0;
            return 1;
        }
        else
        {
            *result = 0;
            return 0; /* not a JSON boolean */
        }
    }

    return 0;
}
