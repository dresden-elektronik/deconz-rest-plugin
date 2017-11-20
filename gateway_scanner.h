#ifndef GATEWAYSCANNER_H
#define GATEWAYSCANNER_H

#include <QObject>
#include <QNetworkReply>
#include <QHostAddress>

class GatewayScannerPrivate;

class GatewayScanner : public QObject
{
    Q_OBJECT
public:
    explicit GatewayScanner(QObject *parent = 0);
    ~GatewayScanner();
    bool isRunning() const;
    void queryGateway(const QString &url);

Q_SIGNALS:
    void foundGateway(const QHostAddress &host, quint16 port, const QString &uuid, const QString &name);

public Q_SLOTS:
    void startScan();

private Q_SLOTS:
    void scanTimerFired();
    void requestFinished(QNetworkReply *reply);
    void onError(QNetworkReply::NetworkError code);

private:
    Q_DECLARE_PRIVATE(GatewayScanner)
    GatewayScannerPrivate *d_ptr;
};

#endif // GATEWAYSCANNER_H
