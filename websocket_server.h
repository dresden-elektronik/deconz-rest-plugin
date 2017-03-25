#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <QObject>
#include <vector>

class QWebSocket;
class QWebSocketServer;

/*! \class WebSocketServer

    Basic websocket server to broadcast messages to clients.
 */
class WebSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketServer(QObject *parent = 0);
    quint16 port() const;

signals:

public slots:
    void broadcastTextMessage(const QString &msg);

private slots:
    void onNewConnection();

private:
    QWebSocketServer *srv;
    std::vector<QWebSocket*> clients;
};

#endif // WEBSOCKET_SERVER_H
