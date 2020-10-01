#include <QTimerEvent>
#include "de_web_plugin_private.h" // todo hack, remove later
#include "device.h"
#include "event.h"
#include "zdp.h"

void DEV_InitStateHandler(Device *device, const Event &event)
{
    if (event.what() != RAttrLastSeen)
    {
        DBG_Printf(DBG_INFO, "DEV Init event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    if (event.what() == REventPoll || event.what() == REventAwake)
    {
        // lazy reference to deCONZ::Node
        if (!device->node())
        {
            device->m_node = getCoreNode(device->key());
        }

        if (device->node())
        {
            device->item(RAttrExtAddress)->setValue(device->node()->address().ext());
            device->item(RAttrNwkAddress)->setValue(device->node()->address().nwk());

            if (device->node()->nodeDescriptor().manufacturerCode() == VENDOR_DDEL && device->node()->address().nwk() == 0x0000)
            {
                return; // ignore coordinaor for now
            }

            // got a node, jump to verification
            device->setState(DEV_NodeDescriptorStateHandler);
        }
        else
        {
            DBG_Printf(DBG_INFO, "DEV Init no node found: 0x%016llX\n", event.deviceKey());
        }
    }
}

void DEV_IdleStateHandler(Device *device, const Event &event)
{
    Q_UNUSED(device)
    if (event.what() == RAttrLastSeen || event.what() == REventPoll)
    {
         // don't print logs
    }
    else if (event.resource() ==  device->prefix())
    {
        DBG_Printf(DBG_INFO, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }
    else
    {
        DBG_Printf(DBG_INFO, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    if (event.what() == REventStateTimeout)
    {
//        device->setState(initStateHandler);
    }
}

void DEV_NodeDescriptorStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        if (!device->node()->nodeDescriptor().isNull())
        {
            DBG_Printf(DBG_INFO, "ZDP node descriptor verified: 0x%016llX\n", device->key());
            device->setState(DEV_ActiveEndpointsStateHandler);
        }
        else
        {
            zdpSendNodeDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), deCONZ::ApsController::instance());
            device->startStateTimer(8000);
        }
    }
    else if (event.what() == REventNodeDescriptor)
    {
        auto *plugin = dynamic_cast<DeRestPluginPrivate*>(device->parent());
        Q_ASSERT(plugin);
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        plugin->enqueueEvent(Event(device->prefix(), REventAwake, 0, device->key()));
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP node descriptor timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        if (!device->node()->endpoints().empty())
        {
            DBG_Printf(DBG_INFO, "ZDP active endpoints verified: 0x%016llX\n", device->key());
            device->setState(DEV_SimpleDescriptorStateHandler);
        }
        else
        {
            zdpSendActiveEndpointsReq(device->item(RAttrNwkAddress)->toNumber(), deCONZ::ApsController::instance());
            device->startStateTimer(8000);
        }
    }
    else if (event.what() == REventActiveEndpoints)
    {
        auto *plugin = dynamic_cast<DeRestPluginPrivate*>(device->parent());
        Q_ASSERT(plugin);
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        plugin->enqueueEvent(Event(device->prefix(), REventAwake, 0, device->key()));
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP active endpoints timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        quint8 needFetchEp = 0x00;

        for (const auto ep : device->node()->endpoints())
        {
            deCONZ::SimpleDescriptor sd;
            if (device->node()->copySimpleDescriptor(ep, &sd) != 0)
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
        else
        {
            zdpSendSimpleDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), needFetchEp, deCONZ::ApsController::instance());
            device->startStateTimer(8000);
        }
    }
    else if (event.what() == REventSimpleDescriptor)
    {
        auto *plugin = dynamic_cast<DeRestPluginPrivate*>(device->parent());
        Q_ASSERT(plugin);
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        plugin->enqueueEvent(Event(device->prefix(), REventAwake, 0, device->key()));
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP simple descriptor timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

void DEV_ModelIdStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        // temp hack to get valid sub device pointers
        auto *plugin = dynamic_cast<DeRestPluginPrivate*>(device->parent());
        Q_ASSERT(plugin);
        Resource *r = nullptr;
        ResourceItem *modelId = nullptr;

        for (const auto &sub : device->m_subDevices)
        {
            r = plugin->getResource(std::get<1>(sub), std::get<0>(sub));
            modelId = r ? r->item(RAttrModelId) : nullptr;
            if (modelId)
            {
                break;
            }
            r = nullptr;
        }

        if (!modelId || device->m_subDevices.empty())
        {
            modelId = device->addItem(DataTypeString, RAttrModelId); // new device?
        }

        if (modelId && !modelId->toString().isEmpty())
        {
            DBG_Printf(DBG_INFO, "modelId verified: 0x%016llX (%s)\n", device->key(), qPrintable(modelId->toString()));
            device->setState(DEV_IdleStateHandler);
        }
        else
        {
            quint8 basicClusterEp = 0x00;

            for (const auto ep : device->node()->endpoints())
            {
                deCONZ::SimpleDescriptor sd;
                if (device->node()->copySimpleDescriptor(ep, &sd) == 0)
                {
                    const auto *cluster = sd.cluster(0x0000, deCONZ::ServerCluster);
                    if (cluster)
                    {
                        basicClusterEp = ep;
                        break;
                    }
                }
            }

            if (basicClusterEp != 0x00 && modelId)
            {
                modelId->setReadParameters({QLatin1String("readGenericAttribute/4"), basicClusterEp, 0x0000, 0x0005, 0x0000});
                modelId->setParseParameters({QLatin1String("parseGenericAttribute/4"), basicClusterEp, 0x0000, 0x0005, "$raw"});
                auto readFunction = getReadFunction(readFunctions, modelId->readParameters());

                if (readFunction && readFunction(device, modelId, deCONZ::ApsController::instance()))
                {

                }
                else
                {
                    DBG_Printf(DBG_INFO, "Failed to read %s: 0x%016llX on endpoint: 0x%02X\n", modelId->descriptor().suffix, device->key(), basicClusterEp);
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "TODO no basic cluster found to read modelId: 0x%016llX\n", device->key());
            }

            device->startStateTimer(8000);
        }
    }
    else if (event.what() == RAttrModelId)
    {
        DBG_Printf(DBG_INFO, "OK received modelId: 0x%016llX\n", device->key());
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler); // ok re-evaluate
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZCL read modelId timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

Device::Device(DeviceKey key, QObject *parent) :
    QObject(parent),
    Resource(RDevices),
    m_deviceKey(key)
{
    addItem(DataTypeUInt64, RAttrExtAddress);
    addItem(DataTypeUInt16, RAttrNwkAddress);
    addItem(DataTypeString, RAttrUniqueId)->setValue(generateUniqueId(key, 0, 0));

    setState(DEV_InitStateHandler);
}

void Device::addSubDevice(const Resource *sub)
{
    Q_ASSERT(sub);
    Q_ASSERT(sub->item(RAttrUniqueId));
    const auto uniqueId = sub->item(RAttrUniqueId)->toString();

    for (const auto &s : m_subDevices)
    {
        if (std::get<0>(s) == uniqueId)
            return; // already registered
    }

    m_subDevices.push_back({uniqueId, sub->prefix()});
}

void Device::handleEvent(const Event &event)
{
    m_state(this, event);
}

void Device::setState(DeviceStateHandler state)
{
    if (m_state != state)
    {
        if (m_state)
        {
            m_state(this, Event(prefix(), REventStateLeave, 0, key()));
        }
        m_state = state;
        if (m_state)
        {
            auto *plugin = dynamic_cast<DeRestPluginPrivate*>(parent());
            Q_ASSERT(plugin);
            // invoke the handler in the next event loop iteration
            plugin->enqueueEvent(Event(prefix(), REventStateEnter, 0, key()));
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
        m_state(this, Event(prefix(), REventStateTimeout, 0, key()));
    }
}

Device *getOrCreateDevice(QObject *parent, DeviceContainer &devices, DeviceKey key)
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
