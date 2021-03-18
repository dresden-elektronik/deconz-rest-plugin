#include <QString>
#include <QUrlQuery>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! NetworkMap REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleNetworkMapApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("networkmap"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/networkmap
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getNetworkMapDatas(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/networkmap
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getNetworkMapDatas(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    QVariantList nodes;
    QVariantList links;
    uint n = 0;
    uint linkId = 0;
    const deCONZ::Node *node = 0;
    while (apsCtrl->getNode(n, &node) == 0)
    {
        QVariantMap currentNode;
        QString className;
        currentNode["id"] = node->address().toStringExt();
        currentNode["name"] = node->userDescriptor();
        currentNode["loaded"] = true;

        if(node->isEndDevice())
        {
            className = "endDevice";
        }
        else
        {
            if(node->isCoordinator())
            {
                className = "coordinator";
            }
            else if(node->isRouter())
            {
                className = "routeur";
            }
            std::vector<deCONZ::NodeNeighbor>::const_iterator nb = node->neighbors().begin();
            std::vector<deCONZ::NodeNeighbor>::const_iterator nbEnd = node->neighbors().end();
            for (; nb != nbEnd; ++nb)
            {
                QVariantMap currentLink;
                QVariantMap currentStyleLink;
                currentLink["id"] = QString::number(linkId);
                currentLink["from"] = node->address().toStringExt();
                currentLink["to"] = nb->address().toStringExt();
                currentStyleLink["toDecoration"] = "arrow";
                currentStyleLink["label"] = QString::number(nb->lqi());
                if(nb->lqi() < 10)
                {
                    currentLink["className"] = "nullLink";
                }
                else if(nb->lqi() < 100)
                {
                    currentLink["className"] = "weakLink";
                }
                else
                {
                    currentLink["className"] = "strongLink";
                }
                currentLink["style"] = currentStyleLink;
                links.append(currentLink);
                linkId++;
            }
        }
        if(node->isZombie())
        {
            className += " zombie";
        }
        currentNode["className"] = className;
        nodes.append(currentNode);
        n++;
    }
    rsp.map["nodes"] = nodes;
    rsp.map["links"] = links;

    return REQ_READY_SEND;
}
