/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef POLL_CONTROL_H
#define POLL_CONTROL_H

// commands send by server
#define POLL_CONTROL_CMD_CHECKIN                 0x00
// commands send by client
#define POLL_CONTROL_CMD_CHECKIN_RESPONSE        0x00
#define POLL_CONTROL_CMD_FAST_POLL_STOP          0x01
#define POLL_CONTROL_CMD_SET_LONG_POLL_INTERVAL  0x02
#define POLL_CONTROL_CMD_SET_SHORT_POLL_INTERVAL 0x03

/* Poll Control cluster

   RStateLastCheckin   book keeping of the last received check-in timestamp
   RConfigCheckin      configuration of the check-in interval
   RConfigLongPoll     configuration of the long poll interval
 */

quint8 PC_GetPollControlEndpoint(const deCONZ::Node *node);

#endif // POLL_CONTROL_H
