#ifndef GATEWAY_H
#define GATEWAY_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkReply>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

namespace deCONZ {
    class ApsDataIndication;
    class ZclFrame;
}

class GatewayPrivate;

class Gateway : public QObject
{
    Q_OBJECT
public:
    class Group
    {
    public:
        QString id;
        QString name;
    };

    class CascadeGroup
    {
    public:
        quint16 local;
        quint16 remote;
    };

    enum State
    {
        StateOffline,
        StateNotAuthorized,
        StateConnected
    };

    explicit Gateway(DeRestPluginPrivate *parent = 0);
    ~Gateway();

    const QString &name() const;
    void setName(const QString &name);
    const QString &uuid() const;
    void setUuid(const QString &uuid);
    const QHostAddress &address() const;
    void setAddress(const QHostAddress &address);
    quint16 port() const;
    void setPort(quint16 port);
    void setApiKey(const QString &apiKey);
    const QString &apiKey() const;
    bool pairingEnabled() const;
    void setPairingEnabled(bool pairingEnabled);
    State state() const;
    bool needSaveDatabase() const;
    void setNeedSaveDatabase(bool save);
    void addCascadeGroup(quint16 local, quint16 remote);
    void removeCascadeGroup(quint16 local, quint16 remote);
    void handleGroupCommand(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);

    const std::vector<Group> &groups() const;
    const std::vector<CascadeGroup> &cascadeGroups() const;

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
