/*
 * Copyright (c) 2018-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Capabilities REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleCapabilitiesApi(const ApiRequest &req, ApiResponse &rsp)
{
    // GET /api/<apikey>/capabilities
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getCapabilities(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/info/timezones
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getCapabilities(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req)

    QVariantMap lightsMap;
    lightsMap[QLatin1String("available")] = static_cast<double>(MAX_NODES - nodes.size());
    lightsMap[QLatin1String("total")] = MAX_NODES;
    rsp.map[QLatin1String("lights")] = lightsMap;

    QVariantMap sensorsMap;
    sensorsMap[QLatin1String("available")] = static_cast<double>(MAX_SENSORS - sensors.size());
    sensorsMap[QLatin1String("total")] = MAX_SENSORS;
    QVariantMap clipMap;
    clipMap[QLatin1String("available")] = static_cast<double>(MAX_SENSORS - sensors.size());
    clipMap[QLatin1String("total")] = MAX_SENSORS;
    sensorsMap[QLatin1String("clip")] = clipMap;
    QVariantMap zllMap;
    zllMap[QLatin1String("available")] = static_cast<double>(MAX_NODES - nodes.size());
    zllMap[QLatin1String("total")] = MAX_NODES;
    sensorsMap[QLatin1String("zll")] = zllMap;
    QVariantMap zgpMap;
    zgpMap[QLatin1String("available")] = static_cast<double>(MAX_NODES - nodes.size());
    zgpMap[QLatin1String("total")] = MAX_NODES;
    sensorsMap[QLatin1String("zgp")] = zgpMap;
    rsp.map[QLatin1String("sensors")] = sensorsMap;

    QVariantMap groupsMap;
    groupsMap[QLatin1String("available")] = static_cast<double>(MAX_GROUPS - groups.size());
    groupsMap[QLatin1String("total")] = MAX_GROUPS;
    rsp.map[QLatin1String("groups")] = groupsMap;

    QVariantMap scenesMap;
    int scenes_size = 0;
    int lightstates_size = 0;
    {
        std::vector<Group>::iterator g = groups.begin();
        std::vector<Group>::iterator g_end = groups.end();
        for (; g != g_end; ++g)
        {
            scenes_size += g->scenes.size();
            std::vector<Scene>::const_iterator s = g->scenes.begin();
            std::vector<Scene>::const_iterator s_end = g->scenes.end();
            for (; s != s_end; ++s)
            {
                lightstates_size += s->lights().size();
            }
        }
    }
    scenesMap[QLatin1String("available")] = MAX_SCENES - scenes_size;
    scenesMap[QLatin1String("total")] = MAX_SCENES;
    QVariantMap lightstatesMap;
    lightstatesMap[QLatin1String("available")] = MAX_LIGHTSTATES - lightstates_size;
    lightstatesMap[QLatin1String("total")] = MAX_LIGHTSTATES;
    scenesMap[QLatin1String("lightstates")] = lightstatesMap;
    rsp.map[QLatin1String("scenes")] = scenesMap;

    QVariantMap schedulesMap;
    schedulesMap[QLatin1String("available")] = static_cast<double>(MAX_SCHEDULES - schedules.size());
    schedulesMap[QLatin1String("total")] = MAX_SCHEDULES;
    rsp.map[QLatin1String("schedules")] = schedulesMap;

    QVariantMap rulesMap;
    int conditions_size = 0;
    int actions_size = 0;
    {
        std::vector<Rule>::const_iterator r = rules.begin();
        std::vector<Rule>::const_iterator r_end = rules.end();
        for (; r != r_end; ++r)
        {
            conditions_size += r->conditions().size();
            actions_size += r->actions().size();
        }
    }
    rulesMap[QLatin1String("available")] = static_cast<double>(MAX_RULES - rules.size());
    rulesMap[QLatin1String("total")] = MAX_RULES;
    QVariantMap conditionsMap;
    conditionsMap[QLatin1String("available")] = MAX_CONDITIONS - conditions_size;
    conditionsMap[QLatin1String("total")] = MAX_CONDITIONS;
    rulesMap[QLatin1String("conditions")] = conditionsMap;
    QVariantMap actionsMap;
    actionsMap[QLatin1String("available")] = MAX_ACTIONS - actions_size;
    actionsMap[QLatin1String("total")] = MAX_ACTIONS;
    rulesMap[QLatin1String("actions")] = actionsMap;
    rsp.map[QLatin1String("rules")] = rulesMap;

    QVariantMap resourcelinksMap;
    resourcelinksMap[QLatin1String("available")] = static_cast<double>(MAX_RESOURCELINKS - resourcelinks.size());
    resourcelinksMap[QLatin1String("total")] = MAX_RESOURCELINKS;
    rsp.map[QLatin1String("resourcelinks")] = resourcelinksMap;

    QVariantMap streamingMap;
    streamingMap[QLatin1String("available")] = MAX_STREAMING;
    streamingMap[QLatin1String("total")] = MAX_STREAMING;
    streamingMap[QLatin1String("channels")] = MAX_CHANNELS;
    rsp.map[QLatin1String("streaming")] = streamingMap;

    QVariantMap tzs;
    tzs["values"] = getTimezones();
    rsp.map["timezones"] = tzs;

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}
