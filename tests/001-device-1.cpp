#include <QCoreApplication>
#include <array>
#include <memory>

// string conversion so catch can print QString
std::ostream& operator << ( std::ostream& os, const QString &str)
{
    os << str.toStdString();
    return os;
}

#include "catch2/catch.hpp"
#include "resource.h"
#include "database.h"
#include "device.h"
#include "device_descriptions.h"
#include "event.h"
#include "utils/utils.h"

int argc = 0;
QCoreApplication app(argc, nullptr);

class MockDeviceparent : public QObject
{
public:

};

class MockNode: public deCONZ::Node
{
    std::vector<deCONZ::NodeNeighbor> m_neighbors;
    deCONZ::BindingTable m_bindingTable;

public:
    deCONZ::CommonState state() const override { return deCONZ::IdleState; }
    const std::vector<deCONZ::NodeNeighbor> &neighbors() const override { return m_neighbors; }
    const deCONZ::BindingTable &bindingTable() const override { return m_bindingTable; }
};

class MockApsController: public deCONZ::ApsController
{
public:
    std::vector<deCONZ::ApsDataRequest> apsReqQueue;

    MockApsController(QObject *parent = nullptr) :
        deCONZ::ApsController(parent)
    {  }

    deCONZ::State networkState() override { return deCONZ::NotInNetwork; }
    int setNetworkState(deCONZ::State) override { return deCONZ::ErrorNotConnected; }
    int setPermitJoin(uint8_t) override { return deCONZ::ErrorNotConnected; }
    int apsdeDataRequest(const deCONZ::ApsDataRequest &req) override {
        UNSCOPED_INFO("apsdeDataRequest() id: " << req.id() << " cluster: " << req.clusterId());
        REQUIRE(req.asdu().size() > 0);
        apsReqQueue.push_back(req);
        return deCONZ::Success;
    }
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

static const DeviceKey DUT_deviceKey = 0x000000A;
static const quint16 nwkAddress = 0xbeaf;

static MockNode node0;
static MockNode *coreNodePtr = nullptr;
MockApsController apsCtrl;
MockDeviceparent parent;

std::unique_ptr<Device> device;

bool DB_StoreSubDevice(const QString &parentUniqueId, const QString &uniqueId)
{
    return !parentUniqueId.isEmpty() && !uniqueId.isEmpty();
}

bool DB_StoreSubDeviceItem(const Resource *sub, const ResourceItem *item)
{
    return sub && item;
}

bool DB_LoadSubDeviceItem(const Resource *sub, ResourceItem *item)
{
    return sub && item;
}


std::vector<DB_ResourceItem> DB_LoadSubDeviceItemsOfDevice(const QString &/*deviceUniqueId*/)
{
    return {};
}

std::vector<DB_ResourceItem> DB_LoadSubDeviceItems(const QString &/*uniqueId*/)
{
    return {};
}


Resource *DEV_InitCompatNodeFromDescription(Device *device, const DeviceDescription::SubDevice &sub, const QString &uniqueId)
{
    Q_UNUSED(device)
    Q_UNUSED(sub)
    Q_UNUSED(uniqueId)

    return nullptr;
}

const deCONZ::Node *DEV_GetCoreNode(uint64_t extAddr)
{
    REQUIRE(coreNodePtr != nullptr);
    REQUIRE(extAddr == coreNodePtr->address().ext());

    return coreNodePtr;
}

Resource *DEV_GetResource(const char *resource, const QString &identifier)
{
    return nullptr;
}

quint8 zclNextSequenceNumber()
{
    return 0;
}

struct DeviceTimerObserver
{
    bool started = false;
    int timeout = -1;
};

std::array<DeviceTimerObserver, StateLevelMax> devTimer;

bool isTimer0Active()
{
    return devTimer[StateLevel0].started;
}

void enqueueEvent(const Event &e)
{
//    INFO("enqueueEvent, what: " << e.what() << " deviceKey: " << e.deviceKey());
//    CHECK(true);

    if (e.what() == REventStartTimer || e.what() == REventStopTimer)
    {
        REQUIRE(EventTimerId(e) < StateLevelMax);
        auto &t = devTimer[EventTimerId(e)];
        t.started = e.what() == REventStartTimer;
        t.timeout = EventTimerTimout(e);

        INFO(e.what() << " timeout: " << t.timeout);
        CHECK(true);
    }

    device->handleEvent(e);
}


TEST_CASE("001: Device constructor", "[Device]")
{
    initResourceDescriptors();

    SECTION("Create new device with default ResourceItems")
    {
        device.reset(new Device(DUT_deviceKey, &apsCtrl, &parent));

        QObject::connect(device.get(), &Device::eventNotify, enqueueEvent);

        REQUIRE(device->item(RStateReachable) != nullptr);
        REQUIRE(device->item(RCapSleeper) != nullptr);
        REQUIRE(device->item(RAttrExtAddress) != nullptr);
        REQUIRE(device->item(RAttrNwkAddress) != nullptr);
        REQUIRE(device->item(RAttrUniqueId) != nullptr);
        REQUIRE(device->item(RAttrManufacturerName) != nullptr);
        REQUIRE(device->item(RAttrModelId) != nullptr);

        REQUIRE(device->item(RAttrUniqueId)->toString() == "00:00:00:00:00:00:00:0a");
        REQUIRE(isTimer0Active() == false);
    }

    SECTION("Assign deCONZ::Node ")
    {
        REQUIRE(device->node() == nullptr);

        coreNodePtr = &node0;
        coreNodePtr->address().setExt(DUT_deviceKey);
        coreNodePtr->address().setNwk(nwkAddress);
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        REQUIRE(device->node() == coreNodePtr);
        REQUIRE(device->item(RAttrExtAddress)->toNumber() == DUT_deviceKey);
        REQUIRE(device->item(RAttrNwkAddress)->toNumber() == nwkAddress);
        REQUIRE(device->node() == coreNodePtr);
        REQUIRE(isTimer0Active() == false);
    }

    SECTION("Update Device state/reachable")
    {
        REQUIRE(device->reachable() == false);
        device->item(RStateReachable)->setValue(true);
        REQUIRE(device->reachable() == true);
    }
}

TEST_CASE("002: Node Descriptor", "[Device]")
{
    SECTION("Query node descriptor")
    {
        // presumly we are in init state with an empty node descriptor
        REQUIRE(device->node()->nodeDescriptor().isNull() == true);

        // poke processing
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // node descriptor state should have been entered and queued a request to the device
        REQUIRE(apsCtrl.apsReqQueue.size() == 1);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_NODE_DESCRIPTOR_CLID);
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle failed confirm")
    {
        device->handleEvent(Event(RDevices, REventApsConfirm, EventApsConfirmPack(apsCtrl.apsReqQueue.back().id(), deCONZ::ApsNoAckStatus), DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // node descriptor state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 2);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_NODE_DESCRIPTOR_CLID);
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle timout on response")
    {
        device->handleEvent(Event(RDevices, REventStateTimeout, 0, DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // node descriptor state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 3);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_NODE_DESCRIPTOR_CLID);
        REQUIRE(isTimer0Active() == true);

        apsCtrl.apsReqQueue.clear();
    }

    SECTION("Handle node descriptor set event")
    {
        const auto raw = QByteArray::fromHex("02408037107f64000000640000");
        QDataStream stream(raw);
        stream.setByteOrder(QDataStream::LittleEndian);
        deCONZ::NodeDescriptor nd;
        nd.readFromStream(stream);
        REQUIRE(nd.isNull() == false);
        coreNodePtr->setNodeDescriptor(nd);

        device->handleEvent(Event(RDevices, REventNodeDescriptor, 0, DUT_deviceKey));
    }
}

TEST_CASE("003: Active Endpoints", "[Device]")
{
    SECTION("Active endpoints state entered")
    {
        // after node descriptor has been set we reached active endpoints state
        REQUIRE(device->node()->nodeDescriptor().isNull() == false);

        // active endpoints state is entered and queued the request
        REQUIRE(apsCtrl.apsReqQueue.size() == 1);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_ACTIVE_ENDPOINTS_CLID);
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle failed confirm")
    {
        device->handleEvent(Event(RDevices, REventApsConfirm, EventApsConfirmPack(apsCtrl.apsReqQueue.back().id(), deCONZ::ApsNoAckStatus), DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // active endpoints state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 2);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_ACTIVE_ENDPOINTS_CLID);
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle timout on response")
    {
        device->handleEvent(Event(RDevices, REventStateTimeout, 0, DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // active endpoints state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 3);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_ACTIVE_ENDPOINTS_CLID);
        REQUIRE(isTimer0Active() == true);

        apsCtrl.apsReqQueue.clear();
    }

    SECTION("Handle active endpoints set event")
    {
        coreNodePtr->setActiveEndpoints({0x01, 0x02});
        device->handleEvent(Event(RDevices, REventActiveEndpoints, 0, DUT_deviceKey));
    }
}

TEST_CASE("004: Simple Descriptors", "[Device]")
{
    SECTION("Simple descriptors state entered")
    {
        // after active endpoints has been set we reached simple descriptors state
        REQUIRE(device->node()->endpoints().empty() == false);

        // simple descriptors state is entered and queued the request
        REQUIRE(apsCtrl.apsReqQueue.size() == 1);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_SIMPLE_DESCRIPTOR_CLID);

        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 4); // seq, nwk address, endpoint
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x01); // first endpoint
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle failed confirm")
    {
        device->handleEvent(Event(RDevices, REventApsConfirm, EventApsConfirmPack(apsCtrl.apsReqQueue.back().id(), deCONZ::ApsNoAckStatus), DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // simple descriptors state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 2);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_SIMPLE_DESCRIPTOR_CLID);
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle timout on response")
    {
        device->handleEvent(Event(RDevices, REventStateTimeout, 0, DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // simple descriptors state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 3);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_SIMPLE_DESCRIPTOR_CLID);
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle first simple descriptor set event")
    {
        const auto raw = QByteArray::fromHex("0104010a000104000003001900010106000004000300050019000101");
        QDataStream stream(raw);
        stream.setByteOrder(QDataStream::LittleEndian);
        deCONZ::SimpleDescriptor sd;
        sd.readFromStream(stream, device->node()->nodeDescriptor().manufacturerCode());
        REQUIRE(sd.isValid() == true);
        coreNodePtr->setSimpleDescriptor(sd);

        apsCtrl.apsReqQueue.clear();

        device->handleEvent(Event(RDevices, REventSimpleDescriptor, 0, DUT_deviceKey));

        // simple descriptors state is entered and queued the request for second entpoint
        REQUIRE(apsCtrl.apsReqQueue.size() == 1);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == ZDP_SIMPLE_DESCRIPTOR_CLID);

        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 4); // seq, nwk address, endpoint
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x02); // second endpoint
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle second simple descriptor set event")
    {
        const auto raw = QByteArray::fromHex("0204010a000104000003001900010106000004000300050019000101");
        QDataStream stream(raw);
        stream.setByteOrder(QDataStream::LittleEndian);
        deCONZ::SimpleDescriptor sd;
        sd.readFromStream(stream, device->node()->nodeDescriptor().manufacturerCode());
        REQUIRE(sd.isValid() == true);
        coreNodePtr->setSimpleDescriptor(sd);

        apsCtrl.apsReqQueue.clear();

        device->handleEvent(Event(RDevices, REventSimpleDescriptor, 0, DUT_deviceKey));
    }
}

TEST_CASE("005: Basic Cluster manufacturer name", "[Device]")
{
    SECTION("Basic cluster state entered")
    {
        // after simple descriptors have been set we reached basic cluster state
        REQUIRE(device->node()->simpleDescriptors().size() == 2);

        REQUIRE(device->item(RAttrManufacturerName)->toString().isEmpty() == true);

        // basic cluster state is entered and queued the request for manufacturer name
        REQUIRE(apsCtrl.apsReqQueue.size() == 1);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == 0x0000);
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 5); // ZCL read attributes
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x04); // manufacturer name attribute (0x0004)
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(4) == 0x00); // manufacturer name attribute
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle failed confirm")
    {
        device->handleEvent(Event(RDevices, REventApsConfirm, EventApsConfirmPack(apsCtrl.apsReqQueue.back().id(), deCONZ::ApsNoAckStatus), DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // basic cluster state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 2);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == 0x0000);
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 5); // ZCL read attributes
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x04); // manufacturer name attribute (0x0004)
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(4) == 0x00); // manufacturer name attribute
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle timout on response")
    {
        device->handleEvent(Event(RDevices, REventStateTimeout, 0, DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // basic cluster state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 3);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == 0x0000);
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 5); // ZCL read attributes
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x04); // manufacturer name attribute (0x0004)
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(4) == 0x00); // manufacturer name attribute
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle manufacturer name set event")
    {
        apsCtrl.apsReqQueue.clear();

        device->item(RAttrManufacturerName)->setValue(QString("IKEA of Sweden"), ResourceItem::SourceDevice);
        device->handleEvent(Event(RDevices, RAttrManufacturerName, 0, DUT_deviceKey));
    }
}

TEST_CASE("006: Basic Cluster modelid", "[Device]")
{
    SECTION("Basic cluster state entered")
    {
        // after simple descriptors have been set we reached basic cluster state
        REQUIRE(device->item(RAttrManufacturerName)->toString().isEmpty() == false);

        // basic cluster state is entered and queued the request for manufacturer name
        REQUIRE(apsCtrl.apsReqQueue.size() == 1);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == 0x0000);
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 5); // ZCL read attributes
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x05); // modelid attribute (0x0005)
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(4) == 0x00); // modelid attribute
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle failed confirm")
    {
        device->handleEvent(Event(RDevices, REventApsConfirm, EventApsConfirmPack(apsCtrl.apsReqQueue.back().id(), deCONZ::ApsNoAckStatus), DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // basic cluster state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 2);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == 0x0000);
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 5); // ZCL read attributes
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x05); // modelid attribute (0x0005)
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(4) == 0x00); // modelid attribute
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle timout on response")
    {
        device->handleEvent(Event(RDevices, REventStateTimeout, 0, DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);

        // back in init state
        // poke processing again
        device->handleEvent(Event(RDevices, REventPoll, 0, DUT_deviceKey));

        // basic cluster state entered again and queued another request
        REQUIRE(apsCtrl.apsReqQueue.size() == 3);
        REQUIRE(apsCtrl.apsReqQueue.back().clusterId() == 0x0000);
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().size() == 5); // ZCL read attributes
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(3) == 0x05); // modelid attribute (0x0005)
        REQUIRE(apsCtrl.apsReqQueue.back().asdu().at(4) == 0x00); // modelid attribute
        REQUIRE(isTimer0Active() == true);
    }

    SECTION("Handle modelid set event")
    {
        apsCtrl.apsReqQueue.clear();

        device->item(RAttrModelId)->setValue(QString("ACME"), ResourceItem::SourceDevice);
        device->handleEvent(Event(RDevices, RAttrModelId, 0, DUT_deviceKey));
        REQUIRE(isTimer0Active() == false);
    }
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
