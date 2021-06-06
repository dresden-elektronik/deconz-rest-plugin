#ifndef DEBUG_H
#define DEBUG_H

#include "deconz/zcl.h"
#include "deconz/aps.h"
#include <QString>
#include <QVariant>
#include <array>

struct KeyValMap {
    QLatin1String key;
    uint8_t value;
    };

struct KeyValMap2 {
    uint8_t key;
    QLatin1String value;
    };

struct KeyValMap3 {
    uint16_t key;
    QLatin1String value;
    };

const std::array<KeyValMap, 16> zclGeneralCommandIds = { { {QLatin1String("Read attributes"), 0x00}, {QLatin1String("Read attributes response"), 0x01}, {QLatin1String("Write attributes"), 0x02},
                                                           {QLatin1String("Write attributes undivided"), 0x03}, {QLatin1String("Write attributes response"), 0x04},
                                                           {QLatin1String("Write attributes no response"), 0x05}, {QLatin1String("Configure reporting"), 0x06},
                                                           {QLatin1String("Configure reporting response"), 0x07}, {QLatin1String("Read reporting config"), 0x08},
                                                           {QLatin1String("Read reporting config response"), 0x09}, {QLatin1String("Report attributes"), 0x0A}, {QLatin1String("Default response"), 0x0B},
                                                           {QLatin1String("Discover attributes"), 0x0C}, {QLatin1String("Discover attributes response"), 0x0D},
                                                           {QLatin1String("Write attributes structured"), 0x0F}, {QLatin1String("Write attributes structured response"), 0x10} } };

const std::array<KeyValMap, 52> zclDataTypes = { { {QLatin1String("NoData"), 0x00}, {QLatin1String("Data8"), 0x08}, {QLatin1String("Data16"), 0x09}, {QLatin1String("Data24"), 0x0A},
                                                   {QLatin1String("Data32"), 0x0B}, {QLatin1String("Data48"), 0x0C}, {QLatin1String("Data56"), 0x0D}, {QLatin1String("Data64"), 0x0E},
                                                   {QLatin1String("Bool"), 0x10}, {QLatin1String("Bit8"), 0x18}, {QLatin1String("Bit16"), 0x19}, {QLatin1String("Bit24"), 0x1A},
                                                   {QLatin1String("Bit32"), 0x1B}, {QLatin1String("Bit40"), 0x1C}, {QLatin1String("Bit48"), 0x1D}, {QLatin1String("Bit56"), 0x1E},
                                                   {QLatin1String("Bit64"), 0x1F}, {QLatin1String("u8"), 0x20}, {QLatin1String("u16"), 0x21}, {QLatin1String("u24"), 0x22},
                                                   {QLatin1String("u32"), 0x23}, {QLatin1String("u40"), 0x24}, {QLatin1String("u48"), 0x25}, {QLatin1String("u56"), 0x26},
                                                   {QLatin1String("u64"), 0x27}, {QLatin1String("i8"), 0x28}, {QLatin1String("i16"), 0x29} , {QLatin1String("i24"), 0x2A},
                                                   {QLatin1String("i32"), 0x2B}, {QLatin1String("i40"), 0x2C}, {QLatin1String("i48"), 0x2D}, {QLatin1String("i56"), 0x2E},
                                                   {QLatin1String("i64"), 0x2F}, {QLatin1String("Enum8"), 0x30}, {QLatin1String("Enum16"), 0x31}, {QLatin1String("Sfloat"), 0x38},
                                                   {QLatin1String("Float"), 0x39} , {QLatin1String("Double"), 0x3A}, {QLatin1String("Ostring"), 0x41}, {QLatin1String("Cstring"), 0x42},
                                                   {QLatin1String("Lostring"), 0x43}, {QLatin1String("Lcstring"), 0x44}, {QLatin1String("Array"), 0x48}, {QLatin1String("Struct"), 0x4C},
                                                   {QLatin1String("Tod"), 0xE0}, {QLatin1String("Date"), 0xE1}, {QLatin1String("Utc"), 0xE2}, {QLatin1String("Clid"), 0xE8},
                                                   {QLatin1String("Attrid"), 0xE9}, {QLatin1String("Bacnetoid"), 0xEA}, {QLatin1String("Ieeeaddr"), 0xF0}, {QLatin1String("128bseckey"), 0xF1} } };



const std::array<KeyValMap2, 21> zclStatusCodes = { { {0x00, QLatin1String("SUCCESS")},
                                                        {0x01, QLatin1String("FAILURE")},
                                                        {0x1C, QLatin1String("SOFTWARE_FAILURE")},
                                                        {0x80, QLatin1String("MALFORMED_COMMAND")},
                                                        {0x81, QLatin1String("UNSUP_CLUSTER_COMMAND")},
                                                        {0x82, QLatin1String("UNSUP_GENERAL_COMMAND")},
                                                        {0x83, QLatin1String("UNSUP_MANUF_CLUSTER_COMMAND")},
                                                        {0x84, QLatin1String("UNSUP_MANUF_GENERAL_COMMAND")},
                                                        {0x85, QLatin1String("INVALID_FIELD")},
                                                        {0x86, QLatin1String("UNSUPPORTED_ATTRIBUTE")},
                                                        {0x87, QLatin1String("INVALID_VALUE")},
                                                        {0x88, QLatin1String("READ_ONLY")},
                                                        {0x89, QLatin1String("INSUFFICIENT_SPACE")},
                                                        {0x8A, QLatin1String("DUPLICATE_EXISTS")},
                                                        {0x8B, QLatin1String("NOT_FOUND")},
                                                        {0x8C, QLatin1String("UNREPORTABLE_ATTRIBUTE")},
                                                        {0x8D, QLatin1String("INVALID_DATA_TYPE")},
                                                        {0x8E, QLatin1String("INVALID_SELECTOR")},
                                                        {0x8F, QLatin1String("WRITE_ONLY")},
                                                        {0x90, QLatin1String("INCONSISTENT_STARTUP_STATE")},
                                                        {0x91, QLatin1String("DEFINED_OUT_OF_BAND")} } };





const std::array<KeyValMap3, 38> zdpCluster = { { {0x0000, QLatin1String("ZDP_NWK_ADDR_CLID")},
                                                    {0x0001, QLatin1String("ZDP_IEEE_ADDR_CLID")},
                                                    {0x0002, QLatin1String("ZDP_NODE_DESCRIPTOR_CLID")},
                                                    {0x0003, QLatin1String("ZDP_POWER_DESCRIPTOR_CLID")},
                                                    {0x0004, QLatin1String("ZDP_SIMPLE_DESCRIPTOR_CLID")},
                                                    {0x0005, QLatin1String("ZDP_ACTIVE_ENDPOINTS_CLID")},
                                                    {0x0006, QLatin1String("ZDP_MATCH_DESCRIPTOR_CLID")},
                                                    {0x0010, QLatin1String("ZDP_COMPLEX_DESCRIPTOR_CLID")},
                                                    {0x0011, QLatin1String("ZDP_USER_DESCRIPTOR_CLID")},
                                                    {0x0013, QLatin1String("ZDP_DEVICE_ANNCE_CLID")},
                                                    {0x0014, QLatin1String("ZDP_USER_DESCRIPTOR_SET_CLID")},
                                                    {0x0020, QLatin1String("ZDP_END_DEVICE_BIND_REQ_CLID")},
                                                    {0x0021, QLatin1String("ZDP_BIND_REQ_CLID")},
                                                    {0x0022, QLatin1String("ZDP_UNBIND_REQ_CLID")},
                                                    {0x0031, QLatin1String("ZDP_MGMT_LQI_REQ_CLID")},
                                                    {0x0032, QLatin1String("ZDP_MGMT_RTG_REQ_CLID")},
                                                    {0x0033, QLatin1String("ZDP_MGMT_BIND_REQ_CLID")},
                                                    {0x0034, QLatin1String("ZDP_MGMT_LEAVE_REQ_CLID")},
                                                    {0x0036, QLatin1String("ZDP_MGMT_PERMIT_JOINING_REQ_CLID")},
                                                    {0x0038, QLatin1String("ZDP_MGMT_NWK_UPDATE_REQ_CLID")},
                                                    {0x8000, QLatin1String("ZDP_NWK_ADDR_RSP_CLID")},
                                                    {0x8001, QLatin1String("ZDP_IEEE_ADDR_RSP_CLID")},
                                                    {0x8002, QLatin1String("ZDP_NODE_DESCRIPTOR_RSP_CLID")},
                                                    {0x8003, QLatin1String("ZDP_POWER_DESCRIPTOR_RSP_CLID")},
                                                    {0x8004, QLatin1String("ZDP_SIMPLE_DESCRIPTOR_RSP_CLID")},
                                                    {0x8005, QLatin1String("ZDP_ACTIVE_ENDPOINTS_RSP_CLID")},
                                                    {0x8006, QLatin1String("ZDP_MATCH_DESCRIPTOR_RSP_CLID")},
                                                    {0x8011, QLatin1String("ZDP_USER_DESCRIPTOR_RSP_CLID")},
                                                    {0x8014, QLatin1String("ZDP_USER_DESCRIPTOR_CONF_CLID")},
                                                    {0x8020, QLatin1String("ZDP_END_DEVICE_BIND_RSP_CLID")},
                                                    {0x8021, QLatin1String("ZDP_BIND_RSP_CLID")},
                                                    {0x8022, QLatin1String("ZDP_UNBIND_RSP_CLID")},
                                                    {0x8031, QLatin1String("ZDP_MGMT_LQI_RSP_CLID")},
                                                    {0x8033, QLatin1String("ZDP_MGMT_BIND_RSP_CLID")},
                                                    {0x8032, QLatin1String("ZDP_MGMT_RTG_RSP_CLID")},
                                                    {0x8034, QLatin1String("ZDP_MGMT_LEAVE_RSP_CLID")},
                                                    {0x8036, QLatin1String("ZDP_MGMT_PERMIT_JOINING_RSP_CLID")},
                                                    {0x8038, QLatin1String("ZDP_MGMT_NWK_UPDATE_RSP_CLID")} } };

void dbgRareGeneralCommand(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgClusterCommand(const deCONZ::ZclFrame &zclFrame, const QString &apsData);
void dbgWriteAttributes(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgWriteAttributesRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgReportAttributesAndReadAttributesRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgReadAttributes(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgReadReportingConfig(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgReadReportingConfigRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgConfigureReporting(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void dbgConfigureReportingRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData);
void printDebugMessage(const QString &debugMessage);

//void dbgReportAttributesAndReadAttributesRsp2(deCONZ::ApsAddressMode adrMode, const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd);


template <typename K, typename Cont>
decltype(auto) matchKeyFromMap1(const K &key, const Cont &cont)
{
    typename Cont::value_type ret{};
    const auto res = std::find_if(cont.cbegin(), cont.cend(), [&key](const auto &i){ return i.key == key; });
    if (res != cont.cend())
    {
        ret = *res;
    }

    return ret;
}

template <typename K, typename Cont>
decltype(auto) matchValueFromMap1(const K &value, const Cont &cont)
{
    typename Cont::value_type ret{};
    const auto res = std::find_if(cont.cbegin(), cont.cend(), [&value](const auto &i){ return i.value == value; });
    if (res != cont.cend())
    {
        ret = *res;
    }

    return ret;
}

#endif // DEBUG_H
