#include <QCoreApplication>
#include "catch2/catch.hpp"
#include "resource.h"
#include "device_js/device_js.h"
#include "device.h"
#include "event.h"

int argc = 0;
QCoreApplication app(argc, nullptr);

class MockNode: public deCONZ::Node
{

};

class MockApsController: public deCONZ::ApsController
{
public:
    MockApsController(QObject *parent = nullptr) :
        deCONZ::ApsController(parent)
    {  }

    deCONZ::State networkState() override { return deCONZ::NotInNetwork; }
    int setNetworkState(deCONZ::State) override { return deCONZ::ErrorNotConnected; }
    int setPermitJoin(uint8_t) override { return deCONZ::ErrorNotConnected; }
    int apsdeDataRequest(const deCONZ::ApsDataRequest&) override { return deCONZ::ErrorNotConnected; }
    int resolveAddress(deCONZ::Address&) override { return deCONZ::ErrorNotFound; }
    int getNode(int, const deCONZ::Node**) override { return -1; }
    bool updateNode(const deCONZ::Node &) override { return false; }
    uint8_t getParameter(deCONZ::U8Parameter) override { return 0; }
    bool setParameter(deCONZ::U8Parameter, uint8_t) override { return false; }
    uint16_t getParameter(deCONZ::U16Parameter) override { return 0; }
    bool setParameter(deCONZ::U16Parameter, uint16_t) override { return false; }
    uint32_t getParameter(deCONZ::U32Parameter) override { return 0; }
    bool setParameter(deCONZ::U32Parameter, uint32_t) override { return false; }
    uint64_t getParameter(deCONZ::U64Parameter) override { return 0; }
    bool setParameter(deCONZ::U64Parameter, uint64_t) override { return false; }
    bool setParameter(deCONZ::ArrayParameter, QByteArray) override { return false; }
    bool setParameter(deCONZ::VariantMapParameter, QVariantMap) override { return false; }
    bool setParameter(deCONZ::StringParameter,  const QString &) override { return false; }
    QString getParameter(deCONZ::StringParameter) override { return {}; }
    QVariantMap getParameter(deCONZ::VariantMapParameter, int) override { return {}; }
    QByteArray getParameter(deCONZ::ArrayParameter) override { return {}; }
    void activateSourceRoute(const deCONZ::SourceRoute &) override {}
    void addBinding(const deCONZ::Binding &) override {}
    void removeBinding(const deCONZ::Binding &) override {}

};

const deCONZ::Node *DEV_GetCoreNode(uint64_t extAddr)
{
    INFO("DEV_GetCoreNode: " << std::hex << extAddr);
    CHECK(extAddr == 10);
    return nullptr;
}

Resource *DEV_GetResource(const char *resource, const QString &identifier)
{
    return nullptr;
}

QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId)
{
    union _a
    {
        quint8 bytes[8];
        quint64 mac;
    } a;
    a.mac = extAddress;
    int ret = -1;
    char buf[64];

    if (clusterId != 0 && endpoint != 0xf2)
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x-%04x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint, clusterId);

    }
    else if (endpoint != 0)
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint);
    }
    else
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0]);
    }
    Q_ASSERT(ret > 0);
    Q_ASSERT(static_cast<size_t>(ret) < sizeof(buf));

    if (ret < 0 || static_cast<size_t>(ret) >= sizeof(buf))
    {
        DBG_Printf(DBG_ERROR, "failed to generate uuid, buffer too small\n");
        Q_ASSERT(0);
        return QString();
    }

    return QString::fromLatin1(buf);
}

quint8 zclNextSequenceNumber()
{
    return 0;
}

TEST_CASE( "001: Basic Math", "[DeviceJs]" )
{
    DeviceJs js;

    REQUIRE(js.evaluate("1 + 2") == JsEvalResult::Ok);

    REQUIRE(js.result().toInt() == 3);

}

TEST_CASE("001: ctor Device", "[Device]")
{
    initResourceDescriptors();

    MockApsController apsCtrl;
    QObject parent;

    DeviceKey deviceKey = 0x000000A;

    Device device(deviceKey, &apsCtrl, &parent);

    REQUIRE(device.item(RStateReachable) != nullptr);
    REQUIRE(device.item(RAttrSleeper) != nullptr);
    REQUIRE(device.item(RAttrExtAddress) != nullptr);
    REQUIRE(device.item(RAttrNwkAddress) != nullptr);
    REQUIRE(device.item(RAttrUniqueId) != nullptr);
    REQUIRE(device.item(RAttrManufacturerName) != nullptr);
    REQUIRE(device.item(RAttrModelId) != nullptr);

    REQUIRE(device.item(RAttrUniqueId)->toString() == "00:00:00:00:00:00:00:0a");

    device.handleEvent(Event(RDevices, REventPoll, 0, deviceKey));

}

TEST_CASE("002: ResourceItem DataTypeBool", "[ResourceItem]")
{
    initResourceDescriptors();

    ResourceItemDescriptor rid;

    REQUIRE(rid.suffix == RInvalidSuffix);

    getResourceItemDescriptor("state/on", rid);

    REQUIRE(rid.suffix == RStateOn);

    ResourceItem item(rid);

    REQUIRE(item.descriptor().suffix == RStateOn);
    REQUIRE(item.descriptor().type == DataTypeBool);

    REQUIRE(item.lastSet().isValid() == false);
    REQUIRE(item.lastChanged().isValid() ==  false);
    REQUIRE(item.toBool() == false);
    REQUIRE(item.toNumber() == 0);
    REQUIRE(item.toVariant().toBool() == false);
    REQUIRE(item.toString() == "");  // TODO
    REQUIRE(item.toVariant().type() == QVariant::Invalid);

    item.setValue(true);

    REQUIRE(item.lastSet().isValid() == true);
    REQUIRE(item.lastChanged().isValid() ==  true);
    REQUIRE(item.toBool() == true);
    REQUIRE(item.toNumber() == 1);
    REQUIRE(item.toVariant().toBool() == true);
    REQUIRE(item.toVariant().type() == QVariant::Bool);
    REQUIRE(item.toString() == "");  // TODO
    REQUIRE(item.toVariant().toString() == "true");
}
