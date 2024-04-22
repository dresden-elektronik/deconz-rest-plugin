/* custom pow() */
static double cj_pow_helper(double base, int exponent)
{
    int i;
    int count;
    double result;

    count = exponent < 0 ? -exponent : exponent;

    result = 1.0;

    if (exponent < 0)
        base = 1.0 / base;

    for (i = 0; i < count; i++)
        result *= base;

    return result;
}

/** Converts a floating point number string to double.
 *
 * The err variable is a bitmap:
 *
 *   0x01 invalid input
 *
 * \param s pointer to string, doesn't have to be '\0' terminated.
 * \param len length of s ala strlen(s).
 * \param endp pointer which will be set to first non 0-9 character (must NOT be NULL).
 * \param err pointer to error variable (must NOT be NULL).
 *
 * \return If the conversion is successful the number is returned and err set to 0.
 *         On failure err has a non zero value.
 */
static double cj_strtod(const char *str, unsigned len, const char **endp, int *err)
{
    int sign;
    int exponent;
    int exp_sign;
    int exp_num;
    int decimal_places;
    double num;
    int required;

    sign = 1;
    exponent = 0;
    exp_sign = 1;
    exp_num = 0;
    decimal_places = 0;
    num = 0.0;
    required = 0;

    /* skip whitespace */
    while (len && (*str == ' ' || *str == '\t'))
    {
        str++;
        len--;
    }

    if (len)
    {
        if (*str == '-')
        {
            sign = -1;
            str++;
            len--;
        }
        else if (*str == '+')
        {
            str++;
            len--;
        }
    }

    /* integer part */
    while (len && *str >= '0' && *str <= '9')
    {
        required = 1;
        num = num * 10 + (*str - '0');
        str++;
        len--;
    }

    /* decimal part */
    if (len && *str == '.')
    {
        str++;
        len--;
        while (len && *str >= '0' && *str <= '9')
        {
            required = 1;
            num = num * 10 + (*str - '0');
            decimal_places++;
            str++;
            len--;
        }
    }

    /* handle exponent */
    if (len && (*str == 'e' || *str == 'E'))
    {
        str++;
        len--;
        if (len)
        {
            if (*str == '-')
            {
                exp_sign = -1;
                str++;
                len--;
            }
            else if (*str == '+')
            {
                str++;
                len--;
            }
        }

        while (len && *str >= '0' && *str <= '9')
        {
            exp_num = exp_num * 10 + (*str - '0');
            str++;
            len--;
        }
        exponent = exp_sign * exp_num;
    }

    /* calculate final result */
    num *= cj_pow_helper(10.0, exponent);
    num /= cj_pow_helper(10.0, decimal_places);

    *endp = str;
    *err = required == 0 ? 1 : 0;

    return sign * num;
}

/** Converts a JSON token reference to double.
 *
 * \param ctx the CJ context.
 * \param result pointer to result double variable.
 * \param ref the token reference of the value.
 *
 * \return 1 on success
 *         0 on failure
 */
int cj_ref_to_double(cj_ctx *ctx, double *result, cj_token_ref ref)
{
    int err;
    cj_token *tok;
    const char *p;
    const char *endptr;

    if (result && ref >= 0 && ref < (cj_token_ref)ctx->tokens_pos)
    {
        tok = &ctx->tokens[ref];
        if (tok->type != CJ_TOKEN_PRIMITIVE || tok->len == 0)
            return 0;

        p = (const char*)&ctx->buf[tok->pos];

        *result = cj_strtod(p, tok->len, &endptr, &err);
        if (endptr == p + tok->len && err == 0)
            return 1;
    }

    return 0;
}

