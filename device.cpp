#include <QTimerEvent>
#include "device.h"
#include "device_descriptions.h"
#include "event.h"
#include "zdp.h"

// TODO move external declaration in de_web_plugin_private.h into utils.h
int getFreeSensorId();
QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);

// enable domain specific string literals
using namespace deCONZ::literals;

/* PlantUML state chart

@startuml
hide empty description
state Init
state "Node Descriptor" as NodeDescriptor
state Endpoints as "Endpoints"
state "Simple Descriptors" as SimpleDescriptors
state "Basic Cluster" as BasicCluster
state "Get DDF" as GetDDF

[*] --> Init
Init --> NodeDescriptor : Reachable or\nHas node Descriptor

NodeDescriptor --> Init : Error
NodeDescriptor --> Endpoints : Has Node Descriptor

Endpoints --> SimpleDescriptors : Has Active Endpoints
Endpoints --> Init : Error

SimpleDescriptors --> BasicCluster : Has Simple Descriptors
SimpleDescriptors --> Init : Error

BasicCluster --> GetDDF
BasicCluster --> Init : Error
note bottom of BasicCluster : read common attributes


GetDDF --> Init : Not found
GetDDF --> Operating : Has DDF

state Operating {
  state Bindings
  ||
  state Scenes
  ||
  state "..."
}


Operating --> Init : Not Reachable
@enduml

*/

constexpr int MinMacPollRxOn = 8000; // 7680 ms + some space for timeout

/*! Returns deCONZ core node for a given \p extAddress.
 */
const deCONZ::Node *DEV_GetCoreNode(uint64_t extAddress)
{
    int i = 0;
    const deCONZ::Node *node = nullptr;
    deCONZ::ApsController *ctrl = deCONZ::ApsController::instance();

    while (ctrl->getNode(i, &node) == 0)
    {
        if (node->address().ext() == extAddress)
        {
            return node;
        }
        i++;
    }

    return nullptr;
}

void DEV_EnqueueEvent(Device *device, const char *event)
{
    Q_ASSERT(device);
    Q_ASSERT(event);
    emit device->eventNotify(Event(device->prefix(), event, 0, device->key()));
}

Resource *DEV_GetSubDevice(Device *device, const char *prefix, const QString &identifier)
{
    for (auto &sub : device->subDevices())
    {
        if (prefix && sub->prefix() != prefix)
        {
            continue;
        }

        if (sub->item(RAttrUniqueId)->toString() == identifier || sub->item(RAttrId)->toString() == identifier)
        {
            return sub;
        }
    }

    return nullptr;
}

void DEV_InitStateHandler(Device *device, const Event &event)
{
    if (event.what() != RAttrLastSeen)
    {
        DBG_Printf(DBG_INFO, "DEV Init event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    if (event.what() == REventPoll ||
        event.what() == REventAwake ||
        event.what() == RConfigReachable ||
        event.what() == REventStateTimeout ||
        event.what() == RStateLastUpdated)
    {
        // lazy reference to deCONZ::Node
        if (!device->node())
        {
            device->m_node = DEV_GetCoreNode(device->key());
        }

        if (device->node())
        {
            device->item(RAttrExtAddress)->setValue(device->node()->address().ext());
            device->item(RAttrNwkAddress)->setValue(device->node()->address().nwk());

            if (device->node()->nodeDescriptor().manufacturerCode_t() == 0x1135_mfcode && device->node()->address().nwk() == 0x0000)
            {
                return; // ignore coordinaor for now
            }

            // got a node, jump to verification
            if (!device->node()->nodeDescriptor().isNull() || device->reachable())
            {
                device->setState(DEV_NodeDescriptorStateHandler);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "DEV Init no node found: 0x%016llX\n", event.deviceKey());
        }
    }
}

void DEV_CheckItemChanges(Device *device, const Event &event)
{
    std::vector<Resource*> subDevices;

    if (event.what() == REventAwake || event.what() == REventPoll)
    {
        subDevices = device->subDevices();
    }
    else
    {
        auto *sub = DEV_GetSubDevice(device, event.resource(), event.id());
        if (sub)
        {
            subDevices.push_back(sub);
        }
    }


    for (auto *sub : subDevices)
    {
        if (sub && !sub->stateChanges().empty())
        {
            auto *item = sub->item(event.what());
            for (auto &change : sub->stateChanges())
            {
                if (item)
                {
                    change.verifyItemChange(item);
                }
                change.tick(sub, deCONZ::ApsController::instance());
            }

            sub->cleanupStateChanges();
        }
    }
}

void DEV_IdleStateHandler(Device *device, const Event &event)
{   
    if (event.what() == RAttrLastSeen || event.what() == REventPoll)
    {
         // don't print logs
    }
    else if (event.what() == REventStateEnter)
    {
        device->setState(DEV_BindingHandler, StateLevel1);
    }
    else if (event.what() == REventStateLeave)
    {
        device->setState(nullptr, StateLevel1);
    }
    else if (event.resource() ==  device->prefix())
    {
        DBG_Printf(DBG_INFO, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }
    else
    {
        DBG_Printf(DBG_INFO, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    DEV_CheckItemChanges(device, event);

    // process parallel states
    if (event.what() != REventStateEnter && event.what() != REventStateLeave)
    {
        device->handleEvent(event, StateLevel1);
    }
}

/*! #2 This state checks that a valid NodeDescriptor is available.
 */
void DEV_NodeDescriptorStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        if (!device->node()->nodeDescriptor().isNull())
        {
            DBG_Printf(DBG_INFO, "ZDP node descriptor verified: 0x%016llX\n", device->key());
            device->setState(DEV_ActiveEndpointsStateHandler);
        }
        else if (!device->reachable()) // can't be queried, go back to #1 init
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            zdpSendNodeDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), deCONZ::ApsController::instance());
            device->startStateTimer(MinMacPollRxOn);
        }
    }
    else if (event.what() == REventNodeDescriptor) // received the node descriptor
    {
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler); // evaluate egain from state #1 init
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP node descriptor timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

/*! #3 This state checks that active endpoints are known.
 */
void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        if (!device->node()->endpoints().empty())
        {
            DBG_Printf(DBG_INFO, "ZDP active endpoints verified: 0x%016llX\n", device->key());
            device->setState(DEV_SimpleDescriptorStateHandler);
        }
        else if (!device->reachable())
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            zdpSendActiveEndpointsReq(device->item(RAttrNwkAddress)->toNumber(), deCONZ::ApsController::instance());
            device->startStateTimer(MinMacPollRxOn);
        }
    }
    else if (event.what() == REventActiveEndpoints)
    {
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP active endpoints timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

/*! #4 This state checks that for all active endpoints simple descriptors are known.
 */
void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        quint8 needFetchEp = 0x00;

        for (const auto ep : device->node()->endpoints())
        {
            deCONZ::SimpleDescriptor sd;
            if (device->node()->copySimpleDescriptor(ep, &sd) != 0 || sd.deviceId() == 0xffff)
            {
                needFetchEp = ep;
                break;
            }
        }

        if (needFetchEp == 0x00)
        {
            DBG_Printf(DBG_INFO, "ZDP simple descriptors verified: 0x%016llX\n", device->key());
            device->setState(DEV_ModelIdStateHandler);
        }
        else if (!device->reachable())
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            zdpSendSimpleDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), needFetchEp, deCONZ::ApsController::instance());
            device->startStateTimer(MinMacPollRxOn);
        }
    }
    else if (event.what() == REventSimpleDescriptor)
    {
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP simple descriptor timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

/*! Returns the first Simple Descriptor for a given server \p clusterId or nullptr if not found.
 */
const deCONZ::SimpleDescriptor *DEV_GetSimpleDescriptorForServerCluster(const Device *device, deCONZ::ZclClusterId_t clusterId)
{
    for (const auto &sd : device->node()->simpleDescriptors())
    {
        const auto cluster = std::find_if(sd.inClusters().cbegin(), sd.inClusters().cend(), [clusterId](const deCONZ::ZclCluster &cl)
        {
            return cl.id_t() == clusterId;
        });

        if (cluster != sd.inClusters().cend())
        {
            return &sd;
        }
    }

    return nullptr;
}

/*! #4 This state checks that modelId of the device is known.
    TODO this should read all common basic cluster attributes needed to match a DDF,
    e.g. modelId, manufacturer name, application version, etc.
 */
void DEV_ModelIdStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        auto *modelId = device->item(RAttrModelId);
        Q_ASSERT(modelId);

        for (const auto rsub : device->subDevices())
        {
            if (!modelId->toString().isEmpty())
            {
                break;
            }

            auto *item = rsub->item(RAttrModelId);
            if (item && !item->toString().isEmpty())
            {
                // copy modelId from sub-device into device
                modelId->setValue(item->toString());
                break;
            }
        }

        if (!modelId->toString().isEmpty())
        {
            DBG_Printf(DBG_INFO, "DEV modelId: %s, 0x%016llX\n", qPrintable(modelId->toString()), device->key());
            device->setState(DEV_GetDeviceDescriptionHandler);
        }
        else if (!device->reachable())
        {
            DBG_Printf(DBG_INFO, "DEV not reachable, check  modelId later: 0x%016llX\n", device->key());
            device->setState(DEV_InitStateHandler);
        }
        else // query modelId from basic cluster
        {
            const auto *sd = DEV_GetSimpleDescriptorForServerCluster(device, 0x0000_clid);

            if (sd)
            {
                modelId->setReadParameters({QLatin1String("readGenericAttribute/4"), sd->endpoint(), 0x0000, 0x0005, 0x0000});
                modelId->setParseParameters({QLatin1String("parseGenericAttribute/4"), sd->endpoint(), 0x0000, 0x0005, "$raw"});
                auto readFunction = getReadFunction(readFunctions, modelId->readParameters());

                if (readFunction && readFunction(device, modelId, deCONZ::ApsController::instance()))
                {
                    device->startStateTimer(MinMacPollRxOn);
                }
                else
                {
                    DBG_Printf(DBG_INFO, "Failed to read %s: 0x%016llX on endpoint: 0x%02X\n", modelId->descriptor().suffix, device->key(), sd->endpoint());
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "TODO no basic cluster found to read modelId: 0x%016llX\n", device->key());
                device->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == RAttrModelId)
    {
        DBG_Printf(DBG_INFO, "DEV received modelId: 0x%016llX\n", device->key());
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler); // ok re-evaluate
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "DEV read modelId timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

QString uniqueIdFromTemplate(const QStringList &templ, const quint64 extAddress)
{
    bool ok = false;
    quint8 endpoint = 0;
    quint16 clusterId = 0;

    // <mac>-<endpoint>
    // <mac>-<endpoint>-<cluster>
    if (templ.size() > 1 && templ.first() == QLatin1String("$address.ext"))
    {
        endpoint = templ.at(1).toUInt(&ok, 0);

        if (ok && templ.size() > 2)
        {
            clusterId = templ.at(2).toUInt(&ok, 0);
        }
    }

    if (ok)
    {
        return generateUniqueId(extAddress, endpoint, clusterId);
    }

    return {};
}

/*! V1 compatibility function to create SensorNodes based on sub-device description.
 */
static Resource *DEV_InitSensorNodeFromDescription(Device *device, const DeviceDescription::SubDevice &sub, const QString &uniqueId)
{
    Sensor sensor;

    sensor.fingerPrint() = sub.fingerPrint;
    sensor.address().setExt(device->item(RAttrExtAddress)->toNumber());
    sensor.address().setNwk(device->item(RAttrNwkAddress)->toNumber());
    sensor.setModelId(device->item(RAttrModelId)->toString());
    sensor.setType(DeviceDescriptions::instance()->constantToString(sub.type));
    sensor.setUniqueId(uniqueId);
    sensor.setNode(const_cast<deCONZ::Node*>(device->node()));
    R_SetValue(&sensor, RConfigOn, true, ResourceItem::SourceApi);

    QString friendlyName = sensor.type();
    if (friendlyName.startsWith("ZHA") || friendlyName.startsWith("ZLL"))
    {
        friendlyName = friendlyName.mid(3);
    }

    sensor.setId(QString::number(getFreeSensorId()));
    sensor.setName(QString("%1 %2").arg(friendlyName, sensor.id()));

    sensor.setNeedSaveDatabase(true);
    sensor.rx();

    auto *r = DEV_AddResource(sensor);
    Q_ASSERT(r);

    device->addSubDevice(r);

    return r;
}

/*! Creates and initialises sub-device Resources and ResourceItems if not already present.

    This function can replace database and joining device initialisation.
 */
static bool DEV_InitDeviceFromDescription(Device *device, const DeviceDescription &description)
{
    Q_ASSERT(device);
    Q_ASSERT(description.isValid());

    size_t subCount = 0;

    for (const auto &sub : description.subDevices)
    {
        Q_ASSERT(sub.isValid());

        const auto uniqueId = uniqueIdFromTemplate(sub.uniqueId, device->item(RAttrExtAddress)->toNumber());

        Resource *rsub = nullptr;

        for (auto *r : device->subDevices())
        {
            if (r->item(RAttrUniqueId)->toString() == uniqueId)
            {
                rsub = r; // already existing Resource* for sub-device
                break;
            }
        }

        if (!rsub && sub.restApi == QLatin1String("/sensors"))
        {
            rsub = DEV_InitSensorNodeFromDescription(device, sub, uniqueId);
        }
        else if (!rsub && sub.restApi == QLatin1String("/lights"))
        {
            // TODO create LightNode for compatibility with v1
        }
        else
        {
            // TODO create dynamic Resource*
        }

        if (rsub)
        {
            subCount++;

            {
                auto *mf = rsub->item(RAttrManufacturerName);
                if (mf && mf->toString().isEmpty())
                {
                    mf->setValue(DeviceDescriptions::instance()->constantToString(description.manufacturer));
                }
            }

            for (const auto &i : sub.items)
            {
                Q_ASSERT(i.isValid());

                auto *item = rsub->item(i.descriptor.suffix);

                if (item)
                {
                    DBG_Printf(DBG_INFO, "sub-device: %s, has item: %s\n", qPrintable(uniqueId), i.descriptor.suffix);
                }
                else
                {
                    DBG_Printf(DBG_INFO, "sub-device: %s, create item: %s\n", qPrintable(uniqueId), i.descriptor.suffix);
                    item = rsub->addItem(i.descriptor.type, i.descriptor.suffix);
                    Q_ASSERT(item);

                    if (i.defaultValue.isValid())
                    {
                        item->setValue(i.defaultValue);
                    }
                }

                if (!item)
                {
                    continue;
                }

                item->setParseParameters(i.parseParameters);
                item->setReadParameters(i.readParameters);
                item->setWriteParameters(i.writeParameters);

                if (item->descriptor().suffix == RConfigCheckin)
                {
                    StateChange stateChange(StateChange::StateWaitSync, SC_WriteZclAttribute, sub.uniqueId.at(1).toUInt());
                    stateChange.addTargetValue(RConfigCheckin, i.defaultValue);
                    stateChange.setChangeTimeoutMs(1000 * 60 * 60);
                    rsub->addStateChange(stateChange);
                }
            }
        }
    }

    return subCount == description.subDevices.size();
}

/*! #5 This state checks if for the modelId a device description is available.
    In that case the device is initialised (or updated) based on the JSON description.
 */
void DEV_GetDeviceDescriptionHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        const auto modelId = device->item(RAttrModelId)->toString();
        const auto description = DeviceDescriptions::instance()->get(device);

        if (description.isValid())
        {
            DBG_Printf(DBG_INFO, "found device description for 0x%016llX, modelId: %s\n", device->key(), qPrintable(modelId));

            DEV_InitDeviceFromDescription(device, description);
            device->setState(DEV_IdleStateHandler); // TODO
        }
        else
        {
            DBG_Printf(DBG_INFO, "No device description for 0x%016llX, modelId: %s\n", device->key(), qPrintable(modelId));
            device->setState(DEV_IdleStateHandler);
        }
    }
}

/*

@startuml
hide empty description

[*] -> Binding
Binding --> TableVerify : Start
TableVerify --> Binding : Done
TableVerify --> TableVerify : Ok, Next
TableVerify --> AddBinding : Missing
TableVerify --> ReadReportConfig : Stale Reports
AddBinding --> ReadReportConfig : Ok
AddBinding --> Binding : Error
ReadReportConfig --> TableVerify : Error or Done
ReadReportConfig --> ReadReportConfig : Ok, Next
ReadReportConfig --> ConfigReporting : Not Found
ConfigReporting --> ReadReportConfig : Error
@enduml
*/

void DEV_BindingHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_INFO, "DEV Binding enter %s/0x%016llX\n", event.resource(), event.deviceKey());
    }

    if (event.what() == REventPoll || event.what() == REventAwake)
    {
        if (!device->m_bindingVerify.isValid() || device->m_bindingVerify.elapsed() > (1000 * 60 * 5))
        {
            DBG_Printf(DBG_INFO, "DEV Binding verify bindings %s/0x%016llX\n", event.resource(), event.deviceKey());
        }
    }
    else if (event.what() == REventBindingTable)
    {
        if (event.num() == deCONZ::ZdpSuccess)
        {
            device->m_mgmtBindSupported = true;
        }
        else if (event.num() == deCONZ::ZdpNotSupported)
        {
            device->m_mgmtBindSupported = false;
        }
    }
    else
    {
        return;
    }

    device->m_bindingIter = 0;
    device->setState(DEV_BindingTableVerifyHandler, StateLevel1);
    DEV_EnqueueEvent(device, REventBindingTick);
}

void DEV_BindingTableVerifyHandler(Device *device, const Event &event)
{
    if (event.what() != REventBindingTick)
    {

    }
    else if (device->m_bindingIter >= device->node()->bindingTable().size())
    {
        device->m_bindingVerify.start();
        device->setState(DEV_BindingHandler, StateLevel1);
    }
    else
    {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto &bnd = *(device->node()->bindingTable().const_begin() + device->m_bindingIter);
        const auto dt = bnd.confirmedMsSinceEpoch() > 0 ? (now - bnd.confirmedMsSinceEpoch()) / 1000: -1;

        if (bnd.dstAddressMode() == deCONZ::ApsExtAddress)
        {
            DBG_Printf(DBG_INFO, "BND 0x%016llX cl: 0x%04X, dstAddrmode: %u, dst: 0x%016llX, dstEp: 0x%02X, dt: %lld seconds\n", bnd.srcAddress(), bnd.clusterId(), bnd.dstAddressMode(), bnd.dstAddress().ext(), bnd.dstEndpoint(), dt);
        }
        else if (bnd.dstAddressMode() == deCONZ::ApsGroupAddress)
        {
            DBG_Printf(DBG_INFO, "BND 0x%016llX cl: 0x%04X, dstAddrmode: %u, group: 0x%04X, dstEp: 0x%02X, dt: %lld seconds\n", bnd.srcAddress(), bnd.clusterId(), bnd.dstAddressMode(), bnd.dstAddress().group(), bnd.dstEndpoint(), dt);
        }

        device->m_bindingIter++;
        DEV_EnqueueEvent(device, REventBindingTick);
    }
}

Device::Device(DeviceKey key, QObject *parent) :
    QObject(parent),
    Resource(RDevices),
    m_deviceKey(key)
{
    Q_ASSERT(parent);
    connect(this, SIGNAL(eventNotify(Event)), parent, SLOT(enqueueEvent(Event)));
    addItem(DataTypeBool, RStateReachable);
    addItem(DataTypeUInt64, RAttrExtAddress);
    addItem(DataTypeUInt16, RAttrNwkAddress);
    addItem(DataTypeString, RAttrUniqueId)->setValue(generateUniqueId(key, 0, 0));
    addItem(DataTypeString, RAttrModelId);

    setState(DEV_InitStateHandler);

    static int initTimer = 1000;
    startStateTimer(initTimer);
    initTimer += 300; // hack for the first round init

    if (deCONZ::appArgumentNumeric("--dev-test-managed", 0) > 0)
    {
        m_managed = true;
    }
}

void Device::addSubDevice(Resource *sub)
{
    Q_ASSERT(sub);
    Q_ASSERT(sub->item(RAttrUniqueId));
    const auto uniqueId = sub->item(RAttrUniqueId)->toString();

    sub->setParentResource(this);

    for (const auto &s : m_subDevices)
    {
        if (std::get<0>(s) == uniqueId)
            return; // already registered
    }

    m_subDevices.push_back({uniqueId, sub->prefix()});
}

void Device::handleEvent(const Event &event, DEV_StateLevel level)
{
    if (event.what() == REventAwake && level == StateLevel0)
    {
        m_awake.start();
    }

    if (m_state[level])
    {
        m_state[level](this, event);
    }
}

void Device::setState(DeviceStateHandler state, DEV_StateLevel level)
{
    if (m_state[level] != state)
    {
        if (m_state[level])
        {
            m_state[level](this, Event(prefix(), REventStateLeave, level, key()));
        }

        m_state[level] = state;

        if (m_state[level])
        {
            if (level == StateLevel0)
            {
                // invoke the handler in the next event loop iteration
                emit eventNotify(Event(prefix(), REventStateEnter, level, key()));
            }
            else
            {
                // invoke sub-states directly
                m_state[level](this, Event(prefix(), REventStateEnter, level, key()));
            }
        }
    }
}

void Device::startStateTimer(int IntervalMs)
{
    m_timer.start(IntervalMs, this);
}

void Device::stopStateTimer()
{
    m_timer.stop();
}

void Device::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer.timerId())
    {
        m_timer.stop(); // single shot
        m_state[StateLevel0](this, Event(prefix(), REventStateTimeout, 0, key()));
    }
}

qint64 Device::lastAwakeMs() const
{
    return m_awake.isValid() ? m_awake.elapsed() : 8640000;
}

bool Device::reachable() const
{
    if (m_awake.isValid() && m_awake.elapsed() < MinMacPollRxOn)
    {
        return true;
    }
    else if (m_node && !m_node->nodeDescriptor().isNull() && m_node->nodeDescriptor().receiverOnWhenIdle())
    {
        return item(RStateReachable)->toBool();
    }

    return false;
}

std::vector<Resource *> Device::subDevices() const
{
    std::vector<Resource *> result;

    // temp hack to get valid sub device pointers
    for (const auto &sub : m_subDevices)
    {
        auto *r = DEV_GetResource(std::get<1>(sub), std::get<0>(sub));

        if (r)
        {
            result.push_back(r);
        }
    }

    return result;
}

Device *DEV_GetDevice(DeviceContainer &devices, DeviceKey key)
{
    auto d = devices.find(key);

    if (d != devices.end())
    {
        return d->second;
    }

    return nullptr;
}

Device *DEV_GetOrCreateDevice(QObject *parent, DeviceContainer &devices, DeviceKey key)
{
    Q_ASSERT(key != 0);
    auto d = devices.find(key);

    if (d == devices.end())
    {
        auto res = devices.insert({key, new Device(key, parent)});
        d = res.first;
        Q_ASSERT(d->second);
    }

    Q_ASSERT(d != devices.end());

    return d->second;
}
