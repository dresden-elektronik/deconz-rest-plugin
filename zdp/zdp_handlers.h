/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef ZDP_HANDLERS_H
#define ZDP_HANDLERS_H


namespace deCONZ {
    class ApsController;
    class ApsDataIndication;
}

void ZDP_HandleNodeDescriptorRequest(const deCONZ::ApsDataIndication &ind, deCONZ::ApsController *apsCtrl);

#endif // ZDP_HANDLERS_H
