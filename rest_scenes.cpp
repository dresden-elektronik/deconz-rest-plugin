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


/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleScenesApi(const ApiRequest &req, ApiResponse &rsp)
{
    // PUT /api/<username>/scenes
    if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("PUT")))
    {
        // return createScene(req, rsp);
    }
    // POST /api/<username>/scenes
    else if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("POST")))
    {
        // return createScene(req, rsp);
    }
    // GET /api/<username>/scenes
    else if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getAllScenes(req, rsp);
    }
    // GET /api/<username>/scenes/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getScene(req, rsp);
    }
    // PUT, PATCH /api/<username>/scenes/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")))
    {
        // return modifyScene(req, rsp);
    }
    // PUT, PATCH /api/<username>/scenes/<id>/lightstates/<id>
    else if ((req.path.size() == 6) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")) && (req.path[4] == QLatin1String("lightstates")))
    {
        // return modifySceneLightState(req, rsp);
    }
    // DELETE /api/<username>/scenes/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("DELETE")))
    {
        // return deleteScene(req, rsp);
    }
    if (rsp.map.isEmpty())
    {
        rsp.str = QLatin1String("{}"); // return empty object
    }
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/scenes
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllScenes(const ApiRequest& req, ApiResponse& rsp)
{
    Q_UNUSED(req);
    std::vector<Group>::const_iterator g = groups.begin();
    std::vector<Group>::const_iterator g_end = groups.end();

    for (; g != g_end; ++g)
    {
        std::vector<Scene>::const_iterator s = g->scenes.begin();
        std::vector<Scene>::const_iterator s_end = g->scenes.end();

        for (; s != s_end; ++s)
        {
            // ignore deleted groups
            if (s->state() == Scene::StateDeleted)
            {
                continue;
            }
            rsp.map[s->id()] = s->map();
        }
    }
    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/scenes/<scene_id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getScene(const ApiRequest& req, ApiResponse& rsp)
{
    const QString resource = "/" + req.path.mid(2).join("/");

    QString id = req.path[3];
    Scene* s = getSceneForId(id);
    if (s) {
        rsp.map = s->map();

        QVariantMap lightstates;
        for (const LightState& l : s->lights())
        {
            lightstates[l.lid()] = l.map();
        }
        rsp.map["lightstates"] = lightstates;
    }
    else
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource, "resource, " + resource + ", not available"));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}
