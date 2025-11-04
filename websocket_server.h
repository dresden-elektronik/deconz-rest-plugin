#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <QObject>
#include <vector>
#ifdef USE_WEBSOCKETS
#include <QWebSocket>
#include <QWebSocketServer>
#endif // USE_WEBSOCKETS

class QWebSocket;
class QWebSocketServer;
class QHttpRequestHeader;

/*! \class WebSocketServer

    Basic websocket server to broadcast messages to clients.
 */
class WebSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketServer(QObject *parent, uint16_t wsPort);
    quint16 port() const;
    void handleExternalTcpSocket(const QHttpRequestHeader &hdr, QTcpSocket *sock);

signals:

public slots:
    void broadcastTextMessage(const QString &msg);
    void flush();

private slots:
    void onNewConnection();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onTextMessageReceived(const QString &message);

private:
    QWebSocketServer *srv;
    std::vector<QWebSocket*> clients;
};

#endif // WEBSOCKET_SERVER_H
