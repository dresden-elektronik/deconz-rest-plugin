/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QUrlQuery>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Map REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleMapApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("map"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/map
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getMap(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/lights
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getMap(const ApiRequest &req, ApiResponse &rsp)
{
    QUrlQuery query(req.hdr.url());

    QString colorCoordinator = query.queryItemValue(QLatin1String("colorCoordinator")).replace(QRegExp("^$"), "green");
    QString colorEndDevice   = query.queryItemValue(QLatin1String("colorEndDevice")).replace(QRegExp("^$"), "blue");
    QString colorRouter      = query.queryItemValue(QLatin1String("colorRouter")).replace(QRegExp("^$"), "black");
    QString colorZombie      = query.queryItemValue(QLatin1String("colorZombie"));
    QString colorBadLink     = query.queryItemValue(QLatin1String("colorBadLink")).replace(QRegExp("^$"), "red");

    // -- links under this threshold are colored differently (using colorBadLink) --
    bool ok;
    int thresBadLink = query.queryItemValue(QLatin1String("thresBadLink")).toInt(&ok);
    if(!ok)
    {
        thresBadLink = 10;
    }

    rsp.httpStatus = HttpStatusOk;

    rsp.str = "digraph G {\n";

    uint n = 0;
    const deCONZ::Node *node = 0;
    while (apsCtrl->getNode(n, &node) == 0)
    {
        QString address = node->address().toStringExt();
        QString name = node->userDescriptor();

        // -- convert address to format 01:23:45:67:89:AB:CD:EF for printing --
        QString addressMAC = "";
        uint64_t a = node->address().ext();
        for (int i = 0; i < 8; i++, a >>= 8)
        {
            addressMAC.push_front(i>0?":":"");
            addressMAC.push_front(QString::number(a & 0xFF, 16).rightJustified(2, '0').toUpper());
        }

        // -- create vertice --
        QString color = "black";
        QString style = "solid";
        if (node->isCoordinator())
        {
            color = colorCoordinator;
        }
        else if (node->isEndDevice())
        {
            color = colorEndDevice;
        }
        else if (node->isRouter())
        {
            color = colorRouter;
        }
        if (node->isZombie() && !colorZombie.isEmpty())
        {
            color = colorZombie;
            style = "dashed";
        }

        QString label = "\"{"+name+"|"+addressMAC+"}\"";
        rsp.str += "\"" + address + "\" [shape=Mrecord label="  +label + ", color=" + color + ", style=" + style + "]\n";

        // -- create edge --
        std::vector<deCONZ::NodeNeighbor>::const_iterator nb = node->neighbors().begin();
        std::vector<deCONZ::NodeNeighbor>::const_iterator nbEnd = node->neighbors().end();
        for (; nb != nbEnd; ++nb)
        {
            quint8 link = nb->lqi();
            QString linkColor = "black";
            if(link < thresBadLink)
            {
                linkColor = colorBadLink;
            }
            rsp.str += "\"" + node->address().toStringExt() +
                    "\" -> \"" + nb->address().toStringExt() +
                    "\" [label=\"" + QString::number(link) + "\", color=" + linkColor + "]\n";
        }

        n++;
    }

    rsp.str += "}";

    return REQ_READY_SEND;
}
