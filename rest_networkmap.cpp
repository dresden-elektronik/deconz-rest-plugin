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
        QVariantMap currentStyleMap;
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

// int DeRestPluginPrivate::getNetworkMapDatas(const ApiRequest &req, ApiResponse &rsp)
// {
//     Q_UNUSED(req);
//     rsp.httpStatus = HttpStatusOk;

//     // handle ETag
//     /*if (req.hdr.hasKey("If-None-Match"))
//     {
//         QString etag = req.hdr.value("If-None-Match");

//         if (sensor->etag == etag)
//         {
//             rsp.httpStatus = HttpStatusNotModified;
//             rsp.etag = etag;
//             return REQ_READY_SEND;
//         }
//     }*/

//     uint n = 0;
//     uint currentEntry = 0;
//     const deCONZ::Node *node = 0;
//     while (apsCtrl->getNode(n, &node) == 0)
//     {
        
//         QVariantMap neighbors;
//         std::vector<deCONZ::NodeNeighbor>::const_iterator nb = node->neighbors().begin();
//         std::vector<deCONZ::NodeNeighbor>::const_iterator nbEnd = node->neighbors().end();
//         for (; nb != nbEnd; ++nb)
//         {
//             neighbors.insert(nb->address().toStringExt(), nb->lqi());
//         }
//         rsp.map[node->address().toStringExt()] = neighbors;
//         /*
//         QVariantMap map;
//         QVariantMap source;
//         QVariantMap dest;
//         QString srcAddress = node->address().toStringExt();


//         QString srcName = node->userDescriptor();
//         QString srcType = "";
//         if(node->isCoordinator())
//         {
//             srcType = "coordinator";
//         }
//         else if(node->isRouter())
//         {
//             srcType = "router";
//         }
//         else if(node->isEndDevice())
//         {
//             srcType = "endDevice";
//         }
//         source["address"] = srcAddress;
//         source["name"] = srcName;
//         source["type"] = srcType;
//         source["isZombie"] = node->isZombie();

//         map["source"] = source;
//         QVariantMap tempMap;
//         tempMap[srcAddress] = source;

//         std::vector<deCONZ::NodeNeighbor>::const_iterator nb = node->neighbors().begin();
//         std::vector<deCONZ::NodeNeighbor>::const_iterator nbEnd = node->neighbors().end();
//         for (; nb != nbEnd; ++nb)
//         {
//             QString dstAddress = nb->address().toStringExt();
//             QString dstName = "dummy";
//             if(tempMap.find(dstAddress) == tempMap.end())
//             {
//                 // not found in tempMap, search for it
//                 dest["address"] = dstAddress;
//                 QVariantMap tempBen = static_cast<QVariantMap>(tempMap[dstAddress]);
//                 dest["name"] = tempBen["name"];
//                 dest["type"] = tempBen["type"];
//                 dest["isZombie"] = tempBen["isZombie"];
//             }
//             else
//             {
//                 uint nTemp = 0;
//                 const deCONZ::Node *nodeTemp = 0;
//                 while (apsCtrl->getNode(nTemp, &nodeTemp) == 0)
//                 {
//                     if(node->address().toStringExt() == dstAddress)
//                     {
//                         dest["address"] = dstAddress;
//                         dest["name"] = nodeTemp->userDescriptor();
//                         QString srcTempType = "";
//                         if(nodeTemp->isCoordinator())
//                         {
//                             srcTempType = "coordinator";
//                         }
//                         else if(nodeTemp->isRouter())
//                         {
//                             srcTempType = "router";
//                         }
//                         else if(nodeTemp->isEndDevice())
//                         {
//                             srcTempType = "endDevice";
//                         }
//                         dest["type"] = srcTempType;
//                         dest["isZombie"] = nodeTemp->isZombie();
//                         tempMap[dstAddress] = dest;
//                     }
//                     nTemp ++;
//                 }
//             }
//             //nb->userDescriptor();
//             quint8 link = nb->lqi();
//             //dest["address"] = dstAddress;
//             //dest["name"] = dstName;
            
//             // update current map
//             map["dest"] = dest;
//             map["lqi"] = link;

//             rsp.map[QString::number(currentEntry)] = map;
//             currentEntry++;
//         }

//         //rsp.map[QString::number(n)] = map;
//         */
//         n++;
//     }

//     if (rsp.map.isEmpty())
//     {
//         rsp.str = "{}"; // return empty object
//     }

//     /*rsp.etag = gwSensorsEtag;*/

//     return REQ_READY_SEND;
// }