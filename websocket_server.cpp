#ifdef USE_WEBSOCKETS
#include <QWebSocket>
#include <QWebSocketServer>

#include "deconz/dbg_trace.h"
#include "websocket_server.h"

/*! Constructor.
 */
WebSocketServer::WebSocketServer(QObject *parent) :
    QObject(parent)
{
    srv = new QWebSocketServer("deconz", QWebSocketServer::NonSecureMode, this);

    if (srv->listen())
    {
        DBG_Printf(DBG_INFO, "started websocket server at port %u\n", srv->serverPort());
    }
    else
    {
        DBG_Printf(DBG_ERROR, "failed to start websocket server %s\n", qPrintable(srv->errorString()));
    }

    connect(srv, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
}

/*! Returns the websocket server port.
    \return the active server port, or 0 if not active
 */
quint16 WebSocketServer::port() const
{
    return srv->isListening() ? srv->serverPort() : 0;
}

/*! Handler for new client connections.
 */
void WebSocketServer::onNewConnection()
{
    while (srv->hasPendingConnections())
    {
        QWebSocket *sock = srv->nextPendingConnection();
        clients.push_back(sock);
    }
}

/*! Broadcasts a message to all connected clients.
    \param msg the message as JSON string
 */
void WebSocketServer::broadcastTextMessage(const QString &msg)
{
    for (size_t i = 0; i < clients.size(); i++)
    {
        QWebSocket *sock = clients[i];

        if (sock->state() == QAbstractSocket::ConnectedState)
        {
            sock->sendTextMessage(msg);
        }
        else
        {
            DBG_Printf(DBG_INFO, "Remove websocket %s:%u\n", sock->peerAddress().toString(), sock->peerPort());
            sock->deleteLater();
            clients[i] = clients.back();
            clients.pop_back();
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
