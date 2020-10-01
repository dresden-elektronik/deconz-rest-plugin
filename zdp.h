#ifndef ZDP_H
#define ZDP_H

#include <deconz.h>

int zdpSendNodeDescriptorReq(uint16_t nwkAddress, deCONZ::ApsController *apsCtrl);
int zdpSendActiveEndpointsReq(uint16_t nwkAddress, deCONZ::ApsController *apsCtrl);
int zdpSendSimpleDescriptorReq(uint16_t nwkAddress, quint8 endpoint, deCONZ::ApsController *apsCtrl);

#endif // ZDP_H
