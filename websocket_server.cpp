/*
 * Copyright (c) 2017-2026 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifdef USE_WEBSOCKETS

#include "deconz/u_assert.h"
#include "deconz/dbg_trace.h"
#include "deconz/util.h"
#include "websocket_server.h"
#include "json.h"

class WebSocketServerPrivate
{
public:
    QWebSocketServer *srv;
    std::vector<QWebSocket*> clients;
};

/*! Constructor.
 */
WebSocketServer::WebSocketServer(QObject *parent, uint16_t wsPort) :
    QObject(parent),
    d_ptr(new WebSocketServerPrivate)
{
    d_ptr->srv = new QWebSocketServer("deconz", QWebSocketServer::NonSecureMode, this);
    QWebSocketServer *srv = d_ptr->srv;

    QHostAddress address;
    QString addrArg = deCONZ::appArgumentString("--http-listen", QString());

    if (addrArg.isEmpty()) // Tie websocket server to specified IP, if any
    {
        address = QHostAddress::AnyIPv4;
    }
    else
    {
        address = QHostAddress(addrArg);
    }

    if (wsPort != 0)
    {
        if (srv->listen(address, wsPort))
        {
            DBG_Printf(DBG_INFO, "Started websocket server on %s, port: %u\n", qPrintable(address.toString()), srv->serverPort());
        }
        else
        {
            DBG_Printf(DBG_ERROR, "Failed starting websocket server on %s, port: %u. error: %s\n", qPrintable(address.toString()), wsPort, qPrintable(srv->errorString()));
        }
    }
    else
    {
        DBG_Printf(DBG_INFO, "Started websocket server on REST-API HTTP(S) ports\n");
    }

    connect(srv, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
}

/*! Adds a socket to the internal WebSocket server which handles the handshake.

    The ownership is transferred to \c srv instance.
 */
void WebSocketServer::handleExternalTcpSocket(const QHttpRequestHeader &hdr, QTcpSocket *sock)
{
    U_ASSERT(sock);
    if (d_ptr && d_ptr->srv)
    {
        d_ptr->srv->handleConnection(sock);
    }
    else
    {
        sock->deleteLater(); // shouldn't happen but don't leak anyway
    }
}

/*! Returns the websocket server port.
    \return the active server port, or 0 if not listening on extra port
 */
quint16 WebSocketServer::port() const
{
    if (!d_ptr || !d_ptr->srv)
        return 0;

    return d_ptr->srv->isListening() ? d_ptr->srv->serverPort() : 0;
}

/*! Handler for new client connections.
 */
void WebSocketServer::onNewConnection()
{
    if (!d_ptr || !d_ptr->srv)
        return;

    while (d_ptr->srv->hasPendingConnections())
    {
        QWebSocket *sock = d_ptr->srv->nextPendingConnection();
        DBG_Printf(DBG_INFO, "New websocket %s:%u\n", qPrintable(sock->peerAddress().toString()), sock->peerPort());
        connect(sock, &QWebSocket::disconnected, this, &WebSocketServer::onSocketDisconnected);
        connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));
        connect(sock, &QWebSocket::textMessageReceived, this, &WebSocketServer::onTextMessageReceived);
        d_ptr->clients.push_back(sock);
    }
}

/*! Handle websocket disconnected signal.
 */
void WebSocketServer::onSocketDisconnected()
{
    if (!d_ptr)
        return;

    auto &clients = d_ptr->clients;
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = qobject_cast<QWebSocket*>(sender());
        DBG_Assert(sock);
        if (sock && clients[i] == sock)
        {
            sock->deleteLater();
            clients[i] = clients.back();
            clients.pop_back();
        }
    }
}

/*! Handle websocket error signal.
    \param err - the error which occured
 */
void WebSocketServer::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    if (!d_ptr)
        return;

    auto &clients = d_ptr->clients;
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = qobject_cast<QWebSocket*>(sender());
        DBG_Assert(sock);
        if (sock && clients[i] == sock)
        {
            sock->deleteLater();
            clients[i] = clients.back();
            clients.pop_back();
        }
    }
}

void WebSocketServer::onTextMessageReceived(const QString &message)
{
    // DBG_Printf(DBG_INFO, "Websocket received: %s\n", qPrintable(message));

    QWebSocket *sock = qobject_cast<QWebSocket*>(sender());
    if (!sock)
        return;

    auto map = Json::parse(message).toMap();

    if (map.isEmpty())
        return;

    // {"enable_aps":true}
    if (map.contains("enable_aps"))
    {
        auto val = map["enable_aps"];
        if (val.type() == QVariant::Bool)
        {
            sock->setProperty("enable_aps", val);
        }
    }

}

/*! Broadcasts a APS indication message to all connected clients which have 'enable_aps' set to true.
    \param msg the message as JSON string
 */
void WebSocketServer::broadcastApsIndication(const QString &msg)
{
    if (!d_ptr)
        return;

    auto &clients = d_ptr->clients;
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = clients[i];
        if (sock->property("enable_aps").toBool())
        {
            sock->sendTextMessage(msg);
            sock->flush();
        }
    }
}

/*! Broadcasts a message to all connected clients.
    \param msg the message as JSON string
 */
void WebSocketServer::broadcastTextMessage(const QString &msg)
{
    if (!d_ptr)
        return;

    auto &clients = d_ptr->clients;
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = clients[i];
        qint64 ret = sock->sendTextMessage(msg);
        // DBG_Printf(DBG_INFO_L2, "Websocket %s:%u send message: %s (ret = %d)\n", qPrintable(sock->peerAddress().toString()), sock->peerPort(), qPrintable(msg), (int)ret);
        sock->flush();
    }
}

/*! Flush the sockets of all connected clients.
 */
void WebSocketServer::flush()
{
    if (!d_ptr)
        return;

    auto &clients = d_ptr->clients;
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = clients[i];

        if (sock->state() == QAbstractSocket::ConnectedState)
        {
            sock->flush();
        }
    }
}

#else // no websockets
  WebSocketServer::WebSocketServer(QObject *parent) :
      QObject(parent)
  { }
  void WebSocketServer::onNewConnection() { }
  void WebSocketServer::broadcastTextMessage(const QString &) { }
  quint16 WebSocketServer::port() const {  return 0; }
#endif
