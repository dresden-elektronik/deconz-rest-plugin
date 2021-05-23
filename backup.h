/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef BACKUP_H
#define BACKUP_H

namespace deCONZ {
    class ApsController;
}

bool BAK_ExportConfiguration(deCONZ::ApsController *apsCtrl);
bool BAK_ImportConfiguration(deCONZ::ApsController *apsCtrl);
bool BAK_ResetConfiguration(deCONZ::ApsController *apsCtrl, bool resetGW, bool deleteDB);

#endif // BACKUP_H
