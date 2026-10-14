/*
 * Copyright (c) 2026 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "utils/utils.h"

/*! Topology REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleTopologyApi(const ApiRequest &req, ApiResponse &rsp)
{
    // GET /api/<apikey>/topology
    if (req.hdr.pathComponentsCount() == 3 && req.hdr.httpMethod() == HttpGet)
    {
        return getTopology(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/topology
    Returns the neighbor table links of all nodes as known by the core.
    Each link is directed: \c source lists \c target as neighbor with the
    given \c lqi as received by \c source.
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getTopology(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    QVariantList links;
    const deCONZ::Node *node = nullptr;

    for (int i = 0; apsCtrl && apsCtrl->getNode(i, &node) == 0; i++)
    {
        for (const deCONZ::NodeNeighbor &neighbor : node->neighbors())
        {
            QVariantMap link;
            link[QLatin1String("source")] = generateUniqueId(node->address().ext(), 0, 0);
            link[QLatin1String("target")] = generateUniqueId(neighbor.address().ext(), 0, 0);
            link[QLatin1String("lqi")] = int(neighbor.lqi());
            links.append(link);
        }
    }

    rsp.map[QLatin1String("links")] = links;
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}
