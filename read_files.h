#ifndef READ_FILES_H
#define READ_FILES_H

#include <sensor.h>

QJsonDocument readButtonMapJson(const QString &path);
bool checkRootLevelObjectsJson(const QJsonDocument &buttonMaps, const QStringList &requiredObjects);
QMap<QString, quint16> loadButtonMapClustersJson(const QJsonDocument &buttonMaps);
QMap<QString, QMap<QString, quint16>> loadButtonMapCommadsJson(const QJsonDocument &buttonMaps);
QMap<QString, QString> loadButtonMapModelIdsJson(const QJsonDocument &buttonMaps);
QMap<QString, std::vector<Sensor::ButtonMap>> loadButtonMapsJson(const QJsonDocument &buttonMaps, const QMap<QString, quint16> &btnMapClusters,
                                                                 const QMap<QString, QMap<QString, quint16>> &btnMapClusterCommands);

#endif // READ_FILES_H
