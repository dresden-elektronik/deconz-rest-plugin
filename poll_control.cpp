/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"
#include "poll_control.h"

static void handleCheckinCommand(DeRestPluginPrivate *plugin, const deCONZ::ApsDataIndication &ind)
{
    std::vector<Resource*> resources;

    for (auto &s : plugin->sensors)
    {
        if (s.address().ext() == ind.srcAddress().ext() && s.deletedState() == Sensor::StateNormal)
        {
            resources.push_back(&s);
            s.setNeedSaveDatabase(true);
        }
    }

    if (!resources.empty())
    {
        plugin->queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
    }

    //        stick to sensors for now, perhaps we need to add lights later on
    //        for (auto &n : plugin->nodes)
    //        {
    //            if (n.address().ext() == ind.srcAddress().ext() && n.state() == LightNode::StateNormal)
    //            {
    //                resources.push_back(&n);
    //            }
    //        }

    const auto now = QDateTime::currentDateTimeUtc();

    for (auto *r : resources)
    {
        auto *item = r->item(RStateLastCheckin);
        if (!item)
        {
            item = r->addItem(DataTypeTime, RStateLastCheckin);
        }

        Q_ASSERT(item);
        if (item)
        {
            item->setIsPublic(false);
            item->setValue(now);
            plugin->enqueueEvent(Event(r->prefix(), item->descriptor().suffix, r->toString(RAttrId), item));
        }
    }

    DBG_Printf(DBG_INFO, "Poll control check-in from 0x%016llX\n", ind.srcAddress().ext());
}

void DeRestPluginPrivate::handlePollControlIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isClusterCommand() && (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) && zclFrame.commandId() == POLL_CONTROL_CMD_CHECKIN)
    {
        handleCheckinCommand(this, ind);
    }
}
