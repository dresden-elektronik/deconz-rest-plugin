#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <sensor.h>
#include <deconz.h>

QJsonDocument readButtonMapJson(const QString &path)
{
    QFile file;
    file.setFileName(path);

    if (file.exists())
    {
        DBG_Printf(DBG_INFO, "[INFO] - Found file containing button maps. Parsing data...\n");

        QJsonParseError error;

        file.open(QIODevice::ReadOnly | QIODevice::Text);
        QJsonDocument buttonMaps = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();

        if (buttonMaps.isNull() || buttonMaps.isEmpty())
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Error: %s at offset: %d (in characters)\n", qPrintable(error.errorString()), error.offset);
            return QJsonDocument();
        }
        else
        {
            return buttonMaps;
        }
    }
    else
    {
        DBG_Printf(DBG_INFO, "[ERROR] - File containing button maps was NOT found.\n");
        return QJsonDocument();
    }
}

bool checkRootLevelObjectsJson(const QJsonDocument &buttonMaps, const QStringList &requiredJsonObjects)
{
    // Check root level objects
    for ( const auto& i : requiredJsonObjects  )
    {
        if (buttonMaps.object().value(QString(i)) == QJsonValue::Undefined)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - No object named '%s' found in JSON file. Skip to load button maps.\n", qPrintable(i));
            return false;
        }
        else if (!buttonMaps.object().value(QString(i)).isObject())
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Expected '%s' in JSON file to be an object, but it isn't. Skip to load button maps.\n", qPrintable(i));
            return false;
        }
    }
    return true;
}

QMap<QString, quint16> loadButtonMapClustersJson(const QJsonDocument &buttonMaps)
{
    // Load button map clusters
    QJsonObject clustersObj = buttonMaps.object().value(QLatin1String("clusters")).toObject();
    QMap<QString, quint16> btnMapClusters;
    quint8 counter = 0;

    for (auto i = clustersObj.constBegin(); i != clustersObj.constEnd(); ++i)       // Loop through cluster objects
    {
        ++counter;

        if (i.key().isNull() || i.key().isEmpty() || i.key().length() > 20)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'clusters' is no string or too long. Skipping entry...\n", counter);
            continue;
        }
        else if (!i.value().isDouble() || i.value().toDouble() > 2000)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Value #%d for object 'clusters' is no number or too large. Skipping entry...\n", counter);
            continue;
        }
        else
        {
            btnMapClusters.insert(i.key(), i.value().toInt());   // Store data in QMap for later use
        }
    }
    return btnMapClusters;
}

QMap<QString, QMap<QString, quint16>> loadButtonMapCommadsJson(const QJsonDocument &buttonMaps)
{
    // Load button map commands
    QJsonObject commandsObj = buttonMaps.object().value(QLatin1String("commands")).toObject();
    QMap<QString, QMap<QString, quint16>> btnMapClusterCommands;
    quint8 counter = 0;

    for (auto i = commandsObj.constBegin(); i != commandsObj.constEnd(); ++i)       // Loop through commands objects
    {
        ++counter;

        if (i.key().isNull() || i.key().isEmpty() || i.key().length() > 20)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'commands' is no string or too long. Skipping entry...\n", counter);
            continue;
        }
        else if (!i.value().isObject())
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Expected '%s' in JSON file to be an object, but it isn't. Skipping entry...\n");
            continue;
        }
        else
        {
            QJsonObject commandObj = i.value().toObject();
            QString commandsObjName = i.key();
            QMap<QString, quint16> commandMap;
            quint8 counter2 = 0;

            for (auto i = commandObj.constBegin(); i != commandObj.constEnd(); ++i)       // Loop through cluster specific command objects
            {
                ++counter2;

                if (i.key().isNull() || i.key().isEmpty() || i.key().length() > 28)
                {
                    DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object '%s' is no string or too long. Skipping entry...\n", counter2, qPrintable(commandsObjName));
                    continue;
                }
                else if (!i.value().isDouble() || i.value().toDouble() > 100)       // FIXME: value might be too small
                {
                    DBG_Printf(DBG_INFO, "[ERROR] - Value #%d for object '%s' is no number or too large. Skipping entry...\n", counter2, qPrintable(commandsObjName));
                    continue;
                }
                else
                {
                    commandMap.insert(i.key(), i.value().toInt());   // Store data in QMap for later use
                }
            }
            btnMapClusterCommands.insert(commandsObjName, commandMap);   // Store data in QMap for later use
        }
    }
    return btnMapClusterCommands;
}

/*! Reads the associated modelIDs from all available button maps in JSON file.
 */
QMap<QString, QString> loadButtonMapModelIdsJson(const QJsonDocument &buttonMaps)
{
    // Load ModelIDs
    QMap<QString, QString> buttonMapForModelId;
    QJsonObject allMapsObj = buttonMaps.object().value(QLatin1String("maps")).toObject();     // Get all button maps

    for (auto i = allMapsObj.constBegin(); i != allMapsObj.constEnd(); ++i)       // Loop through button maps
    {
        QString buttonMapName = i.key();    // Individual button map name

        if (i.value().isObject())        // Check if individual button map is an object
        {
            QJsonObject buttonMapObj = i.value().toObject();
            if (buttonMapObj.value(QString("modelids")).isArray())
            {
                QJsonArray buttonMapModelIds = buttonMapObj.value(QString("modelids")).toArray();

                if (buttonMapModelIds.size() == 0)
                {
                    DBG_Printf(DBG_INFO, "[WARNING] - Button map '%s' in JSON file has no assigned ModelIDs. Skip loading button map...\n", qPrintable(buttonMapName));
                    continue;   // Skip button map
                }

                for (auto i = buttonMapModelIds.constBegin(); i != buttonMapModelIds.constEnd(); ++i)       // Loop through modelIDs
                {
                    QJsonValue val = *i;

                    if (val.isString() && val.toString().size() <= 32)
                    {
                        buttonMapForModelId.insert(val.toString(), buttonMapName);  // Assign button map to modelIDs
                    }
                    else if (val.isString() && val.toString().size() > 32)
                    {
                        DBG_Printf(DBG_INFO, "[ERROR] - Entry of 'modelids', button map '%s' in JSON file is too long. Skipping entry...\n", qPrintable(buttonMapName));
                        continue;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "[ERROR] - Expected entry of 'modelids', button map '%s' in JSON file to be a string, but isn't. Skipping entry...\n", qPrintable(buttonMapName));
                        continue;
                    }
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Expected 'modelids' of button map '%s' in JSON file to be an array, but isn't. Skip loading button map...\n", qPrintable(buttonMapName));
                continue;   // Skip button map
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Expected '%s' in JSON file to be an object, but it isn't. Skip loading button map...\n", qPrintable(buttonMapName));
            continue;   // Skip button map
        }
    }
    return buttonMapForModelId;
}


/*! Reads all available button maps from JSON file.
 */
QMap<QString, std::vector<Sensor::ButtonMap>> loadButtonMapsJson(const QJsonDocument &buttonMaps, const QMap<QString, quint16> &btnMapClusters,
                                                                 const QMap<QString, QMap<QString, quint16>> &btnMapClusterCommands)
{
        QMap<QString, quint16> buttons;
        QMap<QString, quint8> actions;
        quint8 counter = 0;

        // Load button map buttons
        QJsonObject buttonsObj = buttonMaps.object().value(QLatin1String("buttons")).toObject();

        for (auto i = buttonsObj.constBegin(); i != buttonsObj.constEnd(); ++i)       // Loop through button objects
        {
            ++counter;

            if (i.key().isNull() || i.key().isEmpty() || i.key().length() > 11)
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'buttons' is no string or too long. Skipping entry...\n", counter);
                continue;
            }
            else if (!i.value().isDouble() || i.value().toDouble() > 32000)
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Value #%d for object 'buttons' is no number or too large. Skipping entry...\n", counter);
                continue;
            }
            else
            {
                buttons.insert(i.key(), i.value().toInt());   // Store data in QMap for later use
            }
        }

        counter = 0;

        QJsonObject actionsObj = buttonMaps.object().value(QLatin1String("buttonActions")).toObject();

        for (auto i = actionsObj.constBegin(); i != actionsObj.constEnd(); ++i)       // Loop through button action objects
        {
            ++counter;

            if (i.key().isNull() || i.key().isEmpty() || i.key().length() > 33)
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'buttonActions' is no string or too long. Skipping entry...\n", counter);
                continue;
            }
            else if (!i.value().isDouble() || i.value().toDouble() > 16)
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Value #%d for object 'buttonActions' is no number or too large. Skipping entry...\n", counter);
                continue;
            }
            else
            {
                actions.insert(i.key(), i.value().toInt());   // Store data in QMap for later use
            }
        }

        // Load button maps
        QMap<QString, std::vector<Sensor::ButtonMap>> buttonMapData;
        QJsonObject allMapsObj = buttonMaps.object().value(QLatin1String("maps")).toObject();     // Get all button maps

        for (auto i = allMapsObj.constBegin(); i != allMapsObj.constEnd(); ++i)       // Loop through button maps
        {
            QString buttonMapName = i.key();    // Individual button map name
            //DBG_Printf(DBG_INFO, "[INFO] - Button map name: %s\n", qPrintable(buttonMapName));
            quint8 mapItem = 0;

            if (i.value().isObject())        // Check if individual button map is an object
            {
                QJsonObject buttonMapObj = i.value().toObject();

                if (buttonMapObj.value(QString("map")).isArray())   // Check if button map is an array of arrays
                {
                    std::vector<Sensor::ButtonMap> btnMapVec;
                    QJsonArray buttonMapArr = buttonMapObj.value(QString("map")).toArray();
                    //DBG_Printf(DBG_INFO, "[INFO] - Button map size: %d\n", i.value().toArray().size());

                    for (auto i = buttonMapArr.constBegin(); i != buttonMapArr.constEnd(); ++i)       // Loop through button map items
                    {
                        QJsonValue val = *i;
                        if (val.isArray())
                        {
                            QJsonArray buttonMapItemArr = val.toArray();

                            if (buttonMapItemArr.size() != 8)
                            {
                                DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d for '%s' has an incorrect size. Expected 8, got %d\n",
                                            mapItem, qPrintable(buttonMapName), buttonMapItemArr.size());
                                mapItem++;
                                continue;
                            }
                            else
                            {
                                bool ok;
                                quint16 btn = 0;
                                Sensor::ButtonMap btnMap;

                                // Initialize with defaults
                                btnMap.mode = Sensor::ModeNone;
                                btnMap.endpoint = 0;
                                btnMap.clusterId = 0;
                                btnMap.zclCommandId = 0;
                                btnMap.zclParam0 = 0;
                                btnMap.button = 0;
                                btnMap.name = "";

                                if (buttonMapItemArr.at(0).isDouble())
                                {
                                    const auto mode = buttonMapItemArr.at(0).toInt();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #1: %d\n", buttonMapItemArr.at(0).toUint());
                                    if (mode == 0) { btnMap.mode = Sensor::ModeNone; }
                                    else if (mode == 1) { btnMap.mode = Sensor::ModeScenes; }
                                    else if (mode == 2) { btnMap.mode = Sensor::ModeTwoGroups; }
                                    else if (mode == 3) { btnMap.mode = Sensor::ModeColorTemperature; }
                                    else if (mode == 4) { btnMap.mode = Sensor::ModeDimmer; }
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #1 for '%s' does not seem to be an integer.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(1).isString() && buttonMapItemArr.at(1).toString().startsWith(QLatin1String("0x")) &&
                                    buttonMapItemArr.at(1).toString().length() == 4)
                                {
                                    QString ep = buttonMapItemArr.at(1).toString();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #2: %d\n", ep.toUint(&ok, 0));
                                    btnMap.endpoint = ep.toUInt(&ok, 0);
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #2 for '%s' has an incorrect format.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(2).isString() && buttonMapItemArr.at(2).toString().startsWith(QLatin1String("0x")) &&
                                    buttonMapItemArr.at(2).toString().length() == 6)
                                {
                                    QString cid = buttonMapItemArr.at(2).toString();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #3: %d\n", cid.toUint(&ok, 0));
                                    btnMap.clusterId = cid.toUInt(&ok, 0);
                                }
                                else if (buttonMapItemArr.at(2).isString() && !buttonMapItemArr.at(2).toString().startsWith(QLatin1String("0x")) &&
                                         buttonMapItemArr.at(2).toString().length() <= 20)
                                {
                                    QString cid = buttonMapItemArr.at(2).toString();
                                    if (btnMapClusters.value(cid, 65535) != 65535) { btnMap.clusterId = btnMapClusters.value(buttonMapItemArr.at(2).toString()); }
                                    else
                                    {
                                        DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #3 for '%s' was not found in object 'clusters'.\n",
                                                    mapItem, qPrintable(buttonMapName));
                                    }

                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #3 for '%s' has an incorrect format.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(3).isString() && buttonMapItemArr.at(3).toString().startsWith(QLatin1String("0x")) &&
                                    buttonMapItemArr.at(3).toString().length() == 4)
                                {
                                    QString cmd = buttonMapItemArr.at(3).toString();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #4: %d\n", cmd.toUint(&ok, 0));
                                    btnMap.zclCommandId = cmd.toUInt(&ok, 0);
                                }
                                else if (buttonMapItemArr.at(3).isString() && !buttonMapItemArr.at(3).toString().startsWith(QLatin1String("0x")) &&
                                         buttonMapItemArr.at(3).toString().length() <= 28)
                                {
                                    QString cid = buttonMapItemArr.at(2).toString();
                                    QString cmd = buttonMapItemArr.at(3).toString();

                                    if (btnMapClusters.value(cid, 65535) != 65535)
                                    {
                                        QMap<QString, quint16> temp = btnMapClusterCommands.value(cid);

                                        if (!temp.empty() && temp.value(cmd, 65535) != 65535) { btnMap.zclCommandId = temp.value(cmd); }
                                        else
                                        {
                                            DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #4 for '%s' was not found in object 'commands' for cluster '%s'.\n",
                                                        mapItem, qPrintable(buttonMapName), qPrintable(cid));
                                        }
                                    }
                                    else
                                    {
                                        DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #4 for '%s' was not found as cluster in object 'commands'.\n",
                                                    mapItem, qPrintable(buttonMapName));
                                    }
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #4 for '%s' has an incorrect format.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(4).isString() && buttonMapItemArr.at(4).toString().length() <= 3)
                                {
                                    QString para = buttonMapItemArr.at(4).toString();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #5: %d\n", para.toUint(&ok, 0));
                                    btnMap.zclParam0 = para.toUInt(&ok, 0);
                                }
                                else if (buttonMapItemArr.at(4).isString() && buttonMapItemArr.at(4).toString().startsWith(QLatin1String("0x")) &&
                                         (buttonMapItemArr.at(4).toString().length() == 4 || buttonMapItemArr.at(4).toString().length() == 6))
                                {
                                    QString para = buttonMapItemArr.at(4).toString();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #5: %d\n", para.toUint(&ok, 0));
                                    btnMap.zclParam0 = para.toUInt(&ok, 0);
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #5 for '%s' has an incorrect format.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(5).isString() && buttonMapItemArr.at(5).toString().length() <= 11)
                                {
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #6: %s\n", qPrintable(buttonMapItemArr.at(5).toString()));
                                    btn = buttons.value(buttonMapItemArr.at(5).toString(), 0);
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #6 for '%s' is unknown.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(6).isString() && buttonMapItemArr.at(6).toString().length() <= 32)
                                {
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #7: %s\n", qPrintable(buttonMapItemArr.at(6).toString()));
                                    btn += actions.value(buttonMapItemArr.at(6).toString(), 0);
                                    btnMap.button = btn;
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #7 for '%s' is unknown.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                if (buttonMapItemArr.at(7).isString() && buttonMapItemArr.at(7).toString().length() <= 40)
                                {
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #8: %s\n", qPrintable(buttonMapItemArr.at(7).toString()));
                                    btnMap.name = buttonMapItemArr.at(7).toString();
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[INFO] - Button map item #%d, field #8 for '%s' is too long.\n",
                                                mapItem, qPrintable(buttonMapName));
                                }

                                //DBG_Printf(DBG_INFO, "[INFO] - btnMap item #6: %d\n", btnMap.button);
                                //DBG_Printf(DBG_INFO, "[INFO] - btnMap item #7: %s\n", qPrintable(btnMap.name));
                                btnMapVec.push_back(btnMap);
                                mapItem++;
                            }
                        }
                        else
                        {
                            DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d for '%s' in JSON must be an array, but isn't.\n",
                                        mapItem, qPrintable(buttonMapName));

                            mapItem++;
                            continue;
                        }
                    }

                    buttonMapData.insert(buttonMapName, btnMapVec);       // Assign vector of button maps to QMap
                }
                else
                {
                    DBG_Printf(DBG_INFO, "[ERROR] - Expected 'map' of button map '%s' in JSON file to be an array, but isn't. Skip loading button map...\n", qPrintable(buttonMapName));
                    continue;   // Skip button map
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Expected '%s' in JSON file to be an object, but it isn't. Skip loading button map...\n", qPrintable(buttonMapName));
                continue;   // Skip button map
            }
        }

        DBG_Printf(DBG_INFO, "[INFO] - Button maps loaded.\n");

        return buttonMapData;
}
