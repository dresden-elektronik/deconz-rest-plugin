/*
 * Copyright (c) 2021-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <algorithm>
#include "button_maps.h"

ButtonMapRef BM_ButtonMapRefForHash(uint32_t buttonMapNameHash, const std::vector<ButtonMap> &buttonMaps)
{
    const auto bm = std::find_if(buttonMaps.cbegin(), buttonMaps.cend(),
                                 [buttonMapNameHash](const auto &bm) { return bm.buttonMapRef.hash == buttonMapNameHash; });
    if (bm != buttonMaps.cend())
    {
        return bm->buttonMapRef;
    }

    return {};
}

const ButtonMap *BM_ButtonMapForRef(ButtonMapRef ref, const std::vector<ButtonMap> &buttonMaps)
{
    if (isValid(ref) && ref.index < buttonMaps.size())
    {
        const ButtonMap &bm = buttonMaps[ref.index];
        if (bm.buttonMapRef.hash == ref.hash)
        {
            return &bm;
        }
    }

    return nullptr;
}

const ButtonMap *BM_ButtonMapForProduct(ProductIdHash productHash, const std::vector<ButtonMap> &buttonMaps,
                                                    const std::vector<ButtonProduct> &buttonProductMap)
{
    ButtonMapRef buttonMapHash{};
    {
        const auto mapping = std::find_if(buttonProductMap.cbegin(), buttonProductMap.cend(),
                                      [productHash](const auto &i) { return i.productHash == productHash; });

        if (mapping != buttonProductMap.cend())
        {
            buttonMapHash = mapping->buttonMapRef;
        }
    }

    if (isValid(buttonMapHash))
    {
        return BM_ButtonMapForRef(buttonMapHash, buttonMaps);
    }

    return nullptr;
}
