/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef SCRYPT_H
#define SCRYPT_H

#include <string>

struct ScryptParameters
{
    int N;
    int r;
    int p;
    std::string salt;
};

std::string CRYPTO_GenerateSalt();

bool CRYPTO_ParsePhcScryptParameters(const std::string &phcHash, ScryptParameters *param);
std::string CRYPTO_ScryptPassword(const std::string &input, const std::string &salt, int N = 1024, int r = 8, int p = 16);
bool CRYPTO_ScryptVerify(const std::string &phcHash, const std::string &password);

#endif // SCRYPT_H
