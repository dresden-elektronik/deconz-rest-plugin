/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */
#ifndef MMO_HASH_H
#define MMO_HASH_H

#include <string>
#include <vector>

bool CRYPTO_GetMmoHashFromInstallCode(const std::string &hexString, std::vector<unsigned char> &result);

#endif // MMO_HASH_H
