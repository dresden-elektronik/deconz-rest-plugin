#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <deconz/dbg_trace.h>
#include "deconz/atom_table.h"
#include "read_files.h"
#include "sensor.h"

#define MAX_BUTTON_CHARACTER_LENGTH                    11
#define MAX_BUTTON_VALUE                               32000
#define MAX_BUTTON_ACTION_CHARACTER_LENGTH             40
#define MAX_BUTTON_ACTION_VALUE                        64
#define MAX_CLUSTER_CHARACTER_LENGTH                   20
#define MAX_CLUSTER_VALUE                              0xFFFF
#define MAX_COMMAND_CHARACTER_LENGTH                   28
#define MAX_COMMAND_VALUE                              0xFF
#define MAX_MODELID_CHARACTER_LENGTH                   32
#define MAX_DESCRIPTION_CHARACTER_LENGTH               40

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

std::vector<ButtonCluster> loadButtonMapClustersJson(const QJsonDocument &buttonMaps)
{
    int counter = 0;
    AT_AtomIndex ati;
    std::vector<ButtonCluster> result;
    QJsonObject clustersObj = buttonMaps.object().value(QLatin1String("clusters")).toObject();

    for (auto i = clustersObj.constBegin(); i != clustersObj.constEnd(); ++i)       // Loop through cluster objects
    {
        ++counter;

        if (i.key().isNull() || i.key().isEmpty() || i.key().length() > MAX_CLUSTER_CHARACTER_LENGTH)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'clusters' is no string or too long. Skipping entry...\n", counter);
            continue;
        }
        else if (!i.value().isDouble() || i.value().toDouble() > MAX_CLUSTER_VALUE)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Value #%d for object 'clusters' is no number or too large. Skipping entry...\n", counter);
            continue;
        }
        else
        {
            if (AT_AddAtom(qPrintable(i.key()), i.key().size(), &ati))
            {
                ButtonCluster bc;
                bc.nameAtomIndex = ati.index;
                bc.clusterId = i.value().toInt();
                result.push_back(bc);
            }
        }
    }
    return result;
}

std::vector<ButtonClusterCommand> loadButtonMapCommadsJson(const QJsonDocument &buttonMaps)
{
    QJsonObject commandsObj = buttonMaps.object().value(QLatin1String("commands")).toObject();
    std::vector<ButtonClusterCommand> btnMapClusterCommands;
    int counter = 0;
    AT_AtomIndex atiClusterName;
    AT_AtomIndex atiCommandName;

    for (auto i = commandsObj.constBegin(); i != commandsObj.constEnd(); ++i)
    {
        ++counter;
        const std::string clusterName = i.key().toStdString();

        if (clusterName.empty() || clusterName.size() > MAX_CLUSTER_CHARACTER_LENGTH)
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'commands' is no string or too long. Skipping entry...\n", counter);
            continue;
        }
        else if (!i.value().isObject())
        {
            DBG_Printf(DBG_INFO, "[ERROR] - Expected '%s' in JSON file to be an object, but it isn't. Skipping entry...\n", qPrintable(i.key()));
            continue;
        }
        else
        {
            if (AT_AddAtom(clusterName.c_str(), clusterName.size(), &atiClusterName) == 0)
            {
                continue;
            }

            QJsonObject commandObj = i.value().toObject();

            int counter2 = 0;

            for (auto j = commandObj.constBegin(); j != commandObj.constEnd(); ++j)       // Loop through cluster specific command objects
            {
                ++counter2;
                const std::string commandName = j.key().toStdString();
                int commandId = j.value().toInt(-1);

                if (commandName.empty() || commandName.size() > MAX_COMMAND_CHARACTER_LENGTH)
                {
                    DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object '%s' is no string or too long. Skipping entry...\n", counter2, clusterName.c_str());
                    continue;
                }
                else if (commandId < 0 || commandId > MAX_COMMAND_VALUE)       // FIXME: value might be too small
                {
                    DBG_Printf(DBG_INFO, "[ERROR] - Value #%d for object '%s' is no number or too large. Skipping entry...\n", counter2, clusterName.c_str());
                    continue;
                }
                else
                {
                    if (AT_AddAtom(commandName.c_str(), commandName.size(), &atiCommandName) == 0)
                    {
                        continue;
                    }

                    ButtonClusterCommand bcc;
                    bcc.clusterNameAtomIndex = atiClusterName.index;
                    bcc.commandNameAtomIndex = atiCommandName.index;
                    bcc.commandId = (unsigned)commandId;
                    btnMapClusterCommands.push_back(bcc);
                }
            }
        }
    }
    return btnMapClusterCommands;
}

/*! Reads the associated modelIDs from all available button maps in JSON file.
 */
std::vector<ButtonProduct> loadButtonMapModelIdsJson(const QJsonDocument &buttonMapsDoc, const std::vector<ButtonMap> &buttonMaps)
{
    AT_AtomIndex ati;
    std::vector<ButtonProduct> result;

    const QJsonObject allMapsObj = buttonMapsDoc.object().value(QLatin1String("maps")).toObject();     // Get all button maps

    for (auto i = allMapsObj.constBegin(); i != allMapsObj.constEnd(); ++i)       // Loop through button maps
    {
        const QString buttonMapName = i.key();    // Individual button map name

        ButtonProduct item;
        if (AT_GetAtomIndex(qPrintable(buttonMapName), buttonMapName.size(), &ati))
        {
            item.buttonMapRef = BM_ButtonMapRefForHash(ati.index, buttonMaps);
        }
        else
        {
            // atom must be added by earlier pass
            continue;
        }

        if (isValid(item.buttonMapRef) && i.value().isObject())        // Check if individual button map is an object
        {
            const QJsonObject buttonMapObj = i.value().toObject();
            if (buttonMapObj.value(QString("modelids")).isArray())
            {
                const QJsonArray buttonMapModelIds = buttonMapObj.value(QString("modelids")).toArray();

                if (buttonMapModelIds.size() == 0)
                {
                    DBG_Printf(DBG_INFO, "[WARNING] - Button map '%s' in JSON file has no assigned ModelIDs. Skip loading button map...\n", qPrintable(buttonMapName));
                    continue;   // Skip button map
                }

                const auto jend = buttonMapModelIds.constEnd();
                for (auto j = buttonMapModelIds.constBegin(); j != jend; ++j)       // Loop through modelIDs
                {
                    const QString modelId = j->toString();

                    if (j->isString() && !modelId.isEmpty() && modelId.size() <= MAX_MODELID_CHARACTER_LENGTH)
                    {
                        if (AT_AddAtom(qPrintable(modelId), modelId.size(), &ati))
                        {
                            item.productHash = ati.index;
                            result.push_back(item);
                        }
                    }
                    else if (j->isString() && modelId.size() > MAX_MODELID_CHARACTER_LENGTH)
                    {
                        DBG_Printf(DBG_INFO, "[ERROR] - Entry of 'modelids', button map '%s' in JSON file is too long. Skipping entry...\n", qPrintable(buttonMapName));
                        continue;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "[ERROR] - Expected entry of 'modelids', button map '%s' in JSON file to be a string, but isn't. Skipping entry...\n", qPrintable(buttonMapName));
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
    return result;
}


/*! Reads all available button maps from JSON file.
 */
std::vector<ButtonMap> loadButtonMapsJson(const QJsonDocument &buttonMaps, const std::vector<ButtonCluster> &btnMapClusters,
                                                                 const std::vector<ButtonClusterCommand> &btnMapClusterCommands)
{
    AT_AtomIndex ati;
    std::vector<ButtonMap> result;
    result.reserve(128);

        QMap<QString, quint16> buttons;
        QMap<QString, quint8> actions;
        quint8 counter = 0;


        // Load button map buttons
        QJsonObject buttonsObj = buttonMaps.object().value(QLatin1String("buttons")).toObject();

        for (auto i = buttonsObj.constBegin(); i != buttonsObj.constEnd(); ++i)       // Loop through button objects
        {
            ++counter;

            if (i.key().isNull() || i.key().isEmpty() || i.key().length() > MAX_BUTTON_CHARACTER_LENGTH)
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'buttons' is no string or too long. Skipping entry...\n", counter);
                continue;
            }
            else if (!i.value().isDouble() || i.value().toDouble() > MAX_BUTTON_VALUE)
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

            if (i.key().isNull() || i.key().isEmpty() || i.key().length() > MAX_BUTTON_ACTION_CHARACTER_LENGTH)
            {
                DBG_Printf(DBG_INFO, "[ERROR] - Key #%d for object 'buttonActions' is no string or too long. Skipping entry...\n", counter);
                continue;
            }
            else if (!i.value().isDouble() || i.value().toDouble() > MAX_BUTTON_ACTION_VALUE)
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
        QJsonObject allMapsObj = buttonMaps.object().value(QLatin1String("maps")).toObject();     // Get all button maps

        for (auto i = allMapsObj.constBegin(); i != allMapsObj.constEnd(); ++i)       // Loop through button maps
        {
            QString buttonMapName = i.key();    // Individual button map name
            //DBG_Printf(DBG_INFO, "[INFO] - Button map name: %s\n", qPrintable(buttonMapName));
            int mapItem = 0;

            if (i.value().isObject())        // Check if individual button map is an object
            {
                QJsonObject buttonMapObj = i.value().toObject();

                if (buttonMapObj.value(QString("map")).isArray())   // Check if button map is an array of arrays
                {
                    std::vector<ButtonMap::Item> btnMapVec;
                    QJsonArray buttonMapArr = buttonMapObj.value(QString("map")).toArray();
                    //DBG_Printf(DBG_INFO, "[INFO] - Button map size: %d\n", i.value().toArray().size());

                    for (auto i = buttonMapArr.constBegin(); i != buttonMapArr.constEnd(); ++i, mapItem++)       // Loop through button map items
                    {
                        QJsonValue val = *i;
                        if (val.isArray())
                        {
                            QJsonArray buttonMapItemArr = val.toArray();

                            if (buttonMapItemArr.size() != 8)
                            {
                                DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d for '%s' has an incorrect size. Expected 8, got %d\n",
                                            mapItem, qPrintable(buttonMapName), buttonMapItemArr.size());
                                continue;
                            }
                            else
                            {
                                bool ok;
                                quint16 btn = 0;
                                ButtonMap::Item btnMap;

                                // Initialize with defaults
                                btnMap.mode = Sensor::ModeNone;
                                btnMap.endpoint = 0;
                                btnMap.clusterId = 0;
                                btnMap.zclCommandId = 0;
                                btnMap.zclParam0 = 0;
                                btnMap.button = 0;
                                btnMap.nameAtomIndex = 0;

                                if (buttonMapItemArr.at(0).isDouble())
                                {
                                    const auto mode = buttonMapItemArr.at(0).toInt();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #1: %d\n", buttonMapItemArr.at(0).toUint());
                                    if      (mode == 0) { btnMap.mode = Sensor::ModeNone; }
                                    else if (mode == 1) { btnMap.mode = Sensor::ModeScenes; }
                                    else if (mode == 2) { btnMap.mode = Sensor::ModeTwoGroups; }
                                    else if (mode == 3) { btnMap.mode = Sensor::ModeColorTemperature; }
                                    else if (mode == 4) { btnMap.mode = Sensor::ModeDimmer; }
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #1 for '%s' does not seem to be an integer. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
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
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #2 for '%s' has an incorrect format. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
                                }

                                if (buttonMapItemArr.at(2).isString() && buttonMapItemArr.at(2).toString().startsWith(QLatin1String("0x")) &&
                                    buttonMapItemArr.at(2).toString().length() == 6)
                                {
                                    QString cid = buttonMapItemArr.at(2).toString();
                                    unsigned int clusterId = cid.toUInt(&ok, 0);
                                    if (ok && clusterId <= 0xffff)
                                    {
                                        btnMap.clusterId = clusterId;
                                    }
                                }
                                else if (buttonMapItemArr.at(2).isString() && !buttonMapItemArr.at(2).toString().startsWith(QLatin1String("0x")) &&
                                         buttonMapItemArr.at(2).toString().length() <= MAX_CLUSTER_CHARACTER_LENGTH)
                                {
                                    AT_AtomIndex atiClusterName;
                                    bool knownClusterName = false;
                                    std::string clusterName = buttonMapItemArr.at(2).toString().toStdString();

                                    if (AT_GetAtomIndex(clusterName.c_str(), clusterName.size(), &atiClusterName))
                                    {
                                        for (const ButtonCluster &bc : btnMapClusters)
                                        {
                                            if (bc.nameAtomIndex == atiClusterName.index)
                                            {
                                                knownClusterName = true;
                                                btnMap.clusterId = bc.clusterId;
                                                break;
                                            }
                                        }
                                    }

                                    if (!knownClusterName)
                                    {
                                        DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #3 for '%s' was not found in object 'clusters'. Skipping entry.\n",
                                                    mapItem, qPrintable(buttonMapName));
                                        continue;
                                    }
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #3 for '%s' has an incorrect format. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
                                }

                                if (buttonMapItemArr.at(3).isString() && buttonMapItemArr.at(3).toString().startsWith(QLatin1String("0x")) &&
                                    buttonMapItemArr.at(3).toString().length() == 4)
                                {
                                    QString cmd = buttonMapItemArr.at(3).toString();
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #4: %d\n", cmd.toUint(&ok, 0));
                                    btnMap.zclCommandId = cmd.toUInt(&ok, 0);
                                }
                                else if (buttonMapItemArr.at(3).isString() && !buttonMapItemArr.at(3).toString().startsWith(QLatin1String("0x")) &&
                                         buttonMapItemArr.at(3).toString().length() <= MAX_COMMAND_CHARACTER_LENGTH)
                                {
                                    bool found = false;
                                    AT_AtomIndex atiClusterName;
                                    AT_AtomIndex atiCommandName;

                                    std::string clusterName = buttonMapItemArr.at(2).toString().toStdString();
                                    std::string commandName = buttonMapItemArr.at(3).toString().toStdString();

                                    if (AT_GetAtomIndex(clusterName.c_str(), clusterName.size(), &atiClusterName) &&
                                        AT_GetAtomIndex(commandName.c_str(), commandName.size(), &atiCommandName))
                                    {
                                        for (const ButtonClusterCommand &bcc : btnMapClusterCommands)
                                        {
                                            if (bcc.clusterNameAtomIndex == atiClusterName.index &&
                                                bcc.commandNameAtomIndex == atiCommandName.index)
                                            {
                                                btnMap.zclCommandId = bcc.commandId & 0xFF;
                                                found = true;
                                                break;
                                            }
                                        }
                                    }

                                    if (!found)
                                    {
                                        DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, cluster or command for '%s' was not found in object 'commands'. Skipping entry.\n",
                                                    mapItem, qPrintable(buttonMapName));
                                        continue;
                                    }
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #4 for '%s' has an incorrect format. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
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
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #5 for '%s' has an incorrect format. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
                                }

                                if (buttonMapItemArr.at(5).isString() && buttonMapItemArr.at(5).toString().length() <= MAX_BUTTON_CHARACTER_LENGTH)
                                {
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #6: %s\n", qPrintable(buttonMapItemArr.at(5).toString()));
                                    btn = buttons.value(buttonMapItemArr.at(5).toString(), 0);
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #6 for '%s' is unknown. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
                                }

                                if (buttonMapItemArr.at(6).isString() && buttonMapItemArr.at(6).toString().length() <= MAX_BUTTON_ACTION_CHARACTER_LENGTH)
                                {
                                    //DBG_Printf(DBG_INFO, "[INFO] - Button map item #7: %s\n", qPrintable(buttonMapItemArr.at(6).toString()));
                                    btn += actions.value(buttonMapItemArr.at(6).toString(), 0);
                                    btnMap.button = btn;
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #7 for '%s' is unknown. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
                                }

                                if (buttonMapItemArr.at(7).isString() && buttonMapItemArr.at(7).toString().length() <= MAX_DESCRIPTION_CHARACTER_LENGTH)
                                {
                                    AT_AtomIndex atiName;
                                    std::string name = buttonMapItemArr.at(7).toString().toStdString();
                                    btnMap.nameAtomIndex = 0;
                                    if (name.size() != 0)
                                    {
                                        if (AT_AddAtom(name.c_str(), name.size(), &atiName))
                                        {
                                            btnMap.nameAtomIndex = atiName.index;
                                        }
                                    }
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d, field #8 for '%s' is too long. Skipping entry.\n",
                                                mapItem, qPrintable(buttonMapName));
                                    continue;
                                }


                                //DBG_Printf(DBG_INFO, "[INFO] - btnMap item #6: %d\n", btnMap.button);
                                //DBG_Printf(DBG_INFO, "[INFO] - btnMap item #7: %s\n", qPrintable(btnMap.name));
                                btnMapVec.push_back(btnMap);
                            }
                        }
                        else
                        {
                            DBG_Printf(DBG_INFO, "[ERROR] - Button map item #%d for '%s' in JSON must be an array, but isn't.\n",
                                        mapItem, qPrintable(buttonMapName));
                            continue;
                        }
                    }

                    ButtonMapRef buttonMapRef = {};

                    if (AT_AddAtom(qPrintable(buttonMapName), buttonMapName.size(), &ati))
                    {
                        buttonMapRef.hash = ati.index;
                        buttonMapRef.index = result.size();
                    }

#ifdef QT_DEBUG
                    {
                        ButtonMapRef ref = BM_ButtonMapRefForHash(buttonMapRef.hash, result);
                        if (isValid(ref))
                        {
                            DBG_Printf(DBG_INFO, "[ERROR] - Button map duplicated hash for %s\n", qPrintable(buttonMapName));
                        }
                    }
#endif

                    result.emplace_back(ButtonMap{std::move(btnMapVec), buttonMapRef});
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

        return result;
}

std::vector<ButtonMeta> loadButtonMetaJson(const QJsonDocument &buttonMapsDoc, const std::vector<ButtonMap> &buttonMaps)
{
    AT_AtomIndex atiButtonMapName;
    std::vector<ButtonMeta> result;

    const QLatin1String buttonPrefix("S_BUTTON_");
    const QJsonObject mapsObj = buttonMapsDoc.object().value(QLatin1String("maps")).toObject();     // Get all button maps

    for (auto i = mapsObj.constBegin(); i != mapsObj.constEnd(); ++i)       // Loop through button maps
    {
        if (AT_GetAtomIndex(qPrintable(i.key()), i.key().size(), &atiButtonMapName) == 0)
        {
            continue;
        }

        ButtonMeta meta;
        meta.buttonMapRef = BM_ButtonMapRefForHash(atiButtonMapName.index, buttonMaps);    // Individual button map name

        if (!isValid(meta.buttonMapRef))
        {
            continue;
        }

        if (!i.value().isObject())
        {
            continue;
        }

        const QJsonObject buttonMapObj = i.value().toObject();
        if (!buttonMapObj.value(QLatin1String("buttons")).isArray())
        {
            continue;
        }

        const QJsonArray buttons = buttonMapObj.value(QLatin1String("buttons")).toArray();

        for (auto j = buttons.constBegin(); j != buttons.constEnd(); ++j)
        {
            if (!j->isObject())
            {
                continue;
            }

            const QJsonObject buttonObj = j->toObject();
            const auto keys = buttonObj.keys();
            for (const auto &k : keys)
            {
                if (!k.startsWith(buttonPrefix))
                {
                    continue;
                }

                std::string buttonName = buttonObj.value(k).toString().toStdString();
                if (buttonName.size() == 0)
                {
                    continue;
                }

                bool ok = false;
                ButtonMeta::Button b;
                b.button = k.mid(buttonPrefix.size()).toInt(&ok);

                if (ok)
                {
                    AT_AtomIndex atiButtonName;
                    if (AT_AddAtom(buttonName.c_str(), buttonName.size(), &atiButtonName))
                    {
                        b.nameAtomeIndex = atiButtonName.index;
                        meta.buttons.push_back(b);
                    }
                }
            }
        }

        if (!meta.buttons.empty())
        {
            result.push_back(std::move(meta));
        }
    }

    return result;
}
