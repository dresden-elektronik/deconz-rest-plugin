/*
 * Copyright (c)2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef TUYA_H
#define TUYA_H

// Value for dp_type
// ------------------    
// 0x00 	DP_TYPE_RAW 	?
// 0x01 	DP_TYPE_BOOL 	?
// 0x02 	DP_TYPE_VALUE 	4 byte unsigned integer
// 0x03 	DP_TYPE_STRING 	variable length string
// 0x04 	DP_TYPE_ENUM 	1 byte enum
// 0x05 	DP_TYPE_FAULT 	1 byte bitmap (didn't test yet)

#define DP_TYPE_RAW 0x00
#define DP_TYPE_BOOL 0x01
#define DP_TYPE_VALUE 0x02
#define DP_TYPE_STRING 0x03
#define DP_TYPE_ENUM 0x04
#define DP_TYPE_FAULT 0x05

bool isTuyaManufacturerName(const QString &manufacturer);
bool UseTuyaCluster(const QString &manufacturer);

#endif // TUYA_H
