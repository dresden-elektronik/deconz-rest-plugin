/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef IAS_ZONE_H
#define IAS_ZONE_H

#define IAS_ZONE_CLUSTER_ID 0x0500
#define IAS_DEFAULT_ZONE    100

// server send
#define CMD_STATUS_CHANGE_NOTIFICATION 0x00
#define CMD_ZONE_ENROLL_REQUEST        0x01
// server receive
#define CMD_ZONE_ENROLL_RESPONSE       0x00

// Zone status flags
#define STATUS_ALARM1         0x0001
#define STATUS_ALARM2         0x0002
#define STATUS_TAMPER         0x0004
#define STATUS_BATTERY        0x0008
#define STATUS_SUPERVISION    0x0010
#define STATUS_RESTORE_REP    0x0020
#define STATUS_TROUBLE        0x0040
#define STATUS_AC_MAINS       0x0080
#define STATUS_TEST           0x0100
#define STATUS_BATTERY_DEFECT 0x0200

// Attributes
#define IAS_ZONE_STATE        0x0000
#define IAS_ZONE_TYPE         0x0001
#define IAS_ZONE_STATUS       0x0002
#define IAS_CIE_ADDRESS       0x0010
#define IAS_ZONE_ID           0x0011

#endif // IAS_ZONE_H
