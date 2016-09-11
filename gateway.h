#ifndef GATEWAY_H
#define GATEWAY_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkReply>

class GatewayPrivate;
class Gateway : public QObject
{
    Q_OBJECT
public:
    explicit Gateway(QObject *parent = 0);
    ~Gateway();

    const QString &name() const;
    void setName(const QString &name);
    const QString &uuid() const;
    void setUuid(const QString &uuid);
    const QHostAddress &address() const;
    void setAddress(const QHostAddress &address);
    quint16 port() const;
    void setPort(quint16 port);

signals:

private Q_SLOTS:
    void timerFired();
    void finished(QNetworkReply *reply);
    void error(QNetworkReply::NetworkError);

private:
    Q_DECLARE_PRIVATE(Gateway)
    GatewayPrivate *d_ptr;
};

#endif // GATEWAY_H
