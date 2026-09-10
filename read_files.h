#ifndef READ_FILES_H
#define READ_FILES_H

#include <QJsonDocument>
#include "button_maps.h"

QJsonDocument readButtonMapJson(const QString &path);
bool checkRootLevelObjectsJson(const QJsonDocument &buttonMaps, const QStringList &requiredObjects);
std::vector<ButtonCluster> loadButtonMapClustersJson(const QJsonDocument &buttonMaps);
std::vector<ButtonClusterCommand> loadButtonMapCommadsJson(const QJsonDocument &buttonMaps);
std::vector<ButtonProduct> loadButtonMapModelIdsJson(const QJsonDocument &buttonMapsDoc, const std::vector<ButtonMap> &buttonMaps);
std::vector<ButtonMeta> loadButtonMetaJson(const QJsonDocument &buttonMapsDoc, const std::vector<ButtonMap> &buttonMaps);
std::vector<ButtonMap> loadButtonMapsJson(const QJsonDocument &buttonMaps, const std::vector<ButtonCluster> &btnMapClusters, const std::vector<ButtonClusterCommand> &btnMapClusterCommands);

#endif // READ_FILES_H
