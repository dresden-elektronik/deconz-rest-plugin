/*
 * Copyright (c) 2017-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifdef USE_WEBSOCKETS

#include "deconz/dbg_trace.h"
#include "deconz/util.h"
#include "websocket_server.h"

/*! Constructor.
 */
WebSocketServer::WebSocketServer(QObject *parent, quint16 port) :
    QObject(parent)
{
    srv = new QWebSocketServer("deconz", QWebSocketServer::NonSecureMode, this);

    quint16 p = 0;
    quint16 ports[] = { 8087, 20877, 0 }; // start with proxy frinedly ports first, use random port as fallback
    if (port > 0)
    {
        ports[0] = port;
    }

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

    int wsPort = deCONZ::appArgumentNumeric("--ws-port", 0);

    if (wsPort != 0)
    {
        while (!srv->listen(address, ports[p]))
        {
            if (ports[p] == 0)
            {
                DBG_Printf(DBG_ERROR, "Giveup starting websocket server on %s, port: %u. error: %s\n", qPrintable(address.toString()), ports[p], qPrintable(srv->errorString()));
                break;
            }

            DBG_Printf(DBG_ERROR, "Failed to start websocket server on %s, port: %u. error: %s\n", qPrintable(address.toString()), ports[p], qPrintable(srv->errorString()));
            p++;
        }
    }

    if (srv->isListening())
    {
        DBG_Printf(DBG_INFO, "Started websocket server on %s, port: %u\n", qPrintable(address.toString()), srv->serverPort());
    }

    connect(srv, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
}

/*! Adds a socket to the internal WebSocket server which handles the handshake.

    The ownership is transferred to \c srv instance.
 */
void WebSocketServer::handleExternalTcpSocket(const QHttpRequestHeader &hdr, QTcpSocket *sock)
{
    if (srv)
    {
        srv->handleConnection(sock);
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
    return srv && srv->isListening() ? srv->serverPort() : 0;
}

/*! Handler for new client connections.
 */
void WebSocketServer::onNewConnection()
{
    while (srv->hasPendingConnections())
    {
        QWebSocket *sock = srv->nextPendingConnection();
        DBG_Printf(DBG_INFO, "New websocket %s:%u\n", qPrintable(sock->peerAddress().toString()), sock->peerPort());
        connect(sock, &QWebSocket::disconnected, this, &WebSocketServer::onSocketDisconnected);
        connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));
        connect(sock, &QWebSocket::textMessageReceived, this, &WebSocketServer::onTextMessageReceived);
        clients.push_back(sock);
    }
}

/*! Handle websocket disconnected signal.
 */
void WebSocketServer::onSocketDisconnected()
{
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = qobject_cast<QWebSocket*>(sender());
        DBG_Assert(sock);
        if (sock && clients[i] == sock)
        {
            DBG_Printf(DBG_INFO, "Websocket disconnected %s:%u, state: %d, close-code: %d, reason: %s\n", qPrintable(sock->peerAddress().toString()), sock->peerPort(), sock->state(), sock->closeCode(), qPrintable(sock->closeReason()));
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
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = qobject_cast<QWebSocket*>(sender());
        DBG_Assert(sock);
        if (sock && clients[i] == sock)
        {
            DBG_Printf(DBG_INFO, "Remove websocket %s:%u after error %s, close-code: %d, reason: %s\n",
                       qPrintable(sock->peerAddress().toString()), sock->peerPort(), qPrintable(sock->errorString()), sock->closeCode(), qPrintable(sock->closeReason()));
            sock->deleteLater();
            clients[i] = clients.back();
            clients.pop_back();
        }
    }
}

void WebSocketServer::onTextMessageReceived(const QString &message)
{
    Q_UNUSED(message);
    // DBG_Printf(DBG_INFO, "Websocket received: %s\n", qPrintable(message));
}

/*! Broadcasts a message to all connected clients.
    \param msg the message as JSON string
 */
void WebSocketServer::broadcastTextMessage(const QString &msg)
{
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = clients[i];
        qint64 ret = sock->sendTextMessage(msg);
        DBG_Printf(DBG_INFO_L2, "Websocket %s:%u send message: %s (ret = %d)\n", qPrintable(sock->peerAddress().toString()), sock->peerPort(), qPrintable(msg), (int)ret);
        sock->flush();
    }
}

/*! Flush the sockets of all connected clients.
 */
void WebSocketServer::flush()
{
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
