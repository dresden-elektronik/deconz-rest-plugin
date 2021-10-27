/*
 * Copyright (c)2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DOORLOCK_H
#define DOORLOCK_H

#include <sensor.h>

#define OPERATION_EVENT_NOTIFICATON quint8(0x20)
#define PROGRAMMING_EVENT_NOTIFICATON quint8(0x21)

#define COMMAND_READ_PIN quint8(0x06)
#define COMMAND_SET_PIN quint8(0x05)
#define COMMAND_CLEAR_PIN quint8(0x07)

void deletePinEntry(QString &data, quint16 userID);

#endif // DOORLOCK_H
