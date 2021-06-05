#ifndef READ_FILES_H
#define READ_FILES_H

#include <QJsonDocument>
#include "button_maps.h"

QJsonDocument readButtonMapJson(const QString &path);
bool checkRootLevelObjectsJson(const QJsonDocument &buttonMaps, const QStringList &requiredObjects);
QMap<QString, quint16> loadButtonMapClustersJson(const QJsonDocument &buttonMaps);
QMap<QString, QMap<QString, quint16>> loadButtonMapCommadsJson(const QJsonDocument &buttonMaps);
std::vector<ButtonProduct> loadButtonMapModelIdsJson(const QJsonDocument &buttonMapsDoc, const std::vector<ButtonMap> &buttonMaps);
std::vector<ButtonMeta> loadButtonMetaJson(const QJsonDocument &buttonMapsDoc, const std::vector<ButtonMap> &buttonMaps);
std::vector<ButtonMap> loadButtonMapsJson(const QJsonDocument &buttonMaps, const QMap<QString, quint16> &btnMapClusters, const QMap<QString, QMap<QString, quint16>> &btnMapClusterCommands);

#endif // READ_FILES_H
