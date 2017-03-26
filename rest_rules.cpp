/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QVariantMap>
#include <QRegExp>
#include <QStringBuilder>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

#define MAX_RULES_COUNT 500
#define RTYPE_NONE 0x01
#define RTYPE_BOOL 0x02
#define RTYPE_INT  0x04

struct SensorResourceDescriptor {
    const char *sensorType;
    const char *resource;
    const char *operators;
    quint8 type;
};

static const SensorResourceDescriptor  resourceDescriptors[] = {
    { "ZHAPresence",       "/state/presence",      "eq",       RTYPE_BOOL },
    { "CLIPPresence",      "/state/presence",      "eq",       RTYPE_BOOL },
    { "CLIPOpenClose",     "/state/open",          "eq",       RTYPE_BOOL },
    { "ZHALight",          "/state/dark",          "eq",       RTYPE_BOOL },
    { "ZHALight",          "/state/lux",           "eq gt lt", RTYPE_INT },
    { "ZHALight",          "/state/lightlevel",    "eq gt lt", RTYPE_INT },
    { "ZHASwitch",         "/state/buttonevent",   "eq gt lt", RTYPE_INT },
    { "ZHATemperature",    "/state/temperature",   "eq gt lt", RTYPE_INT },
    { "ZHAHumidity",       "/state/humidity",      "eq gt lt", RTYPE_INT },
    { "CLIPSwitch",        "/state/buttonevent",   "eq gt lt", RTYPE_INT },
    { "CLIPTemperature",   "/state/temperature",   "eq gt lt", RTYPE_INT },
    { "CLIPHumidity",      "/state/humidity",      "eq gt lt", RTYPE_INT },
    { "CLIPGenericFlag",   "/state/flag",          "eq",       RTYPE_BOOL },
    { "CLIPGenericStatus", "/state/status",        "eq gt lt", RTYPE_INT },
    { "0",                 "/state/lastupdated",   "dx",       RTYPE_NONE },
    { "0",                 "/config/on",           "eq",       RTYPE_BOOL },
    { "0",                 "/config/reachable",    "eq",       RTYPE_BOOL },
    { 0,                   0,                      0,          0 }
};

/*! Rules REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleRulesApi(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    // GET /api/<apikey>/rules
    if ((req.path.size() == 3) && (req.hdr.method() == "GET")  && (req.path[2] == "rules"))
    {
        return getAllRules(req, rsp);
    }
    // GET /api/<apikey>/rules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET") && (req.path[2] == "rules"))
    {
        return getRule(req, rsp);
    }
    // POST /api/<apikey>/rules
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST") && (req.path[2] == "rules"))
    {
        return createRule(req, rsp);
    }
    // PUT /api/<apikey>/rules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT") && (req.path[2] == "rules"))
    {
        return updateRule(req, rsp);
    }
    // DELETE /api/<apikey>/rules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "DELETE") && (req.path[2] == "rules"))
    {
        return deleteRule(req, rsp);
    }

    return REQ_NOT_HANDLED;
}


/*! GET /api/<apikey>/rules
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllRules(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    std::vector<Rule>::const_iterator i = rules.begin();
    std::vector<Rule>::const_iterator end = rules.end();

    for (; i != end; ++i)
    {
        // ignore deleted rules
        if (i->state() == Rule::StateDeleted)
        {
            continue;
        }

        QVariantMap rule;

        std::vector<RuleCondition>::const_iterator c = i->conditions().begin();
        std::vector<RuleCondition>::const_iterator cend = i->conditions().end();

        QVariantList conditions;

        for (; c != cend; ++c)
        {
            QVariantMap condition;
            condition["address"] = c->address();
            condition["operator"] = c->ooperator();
            if (c->value().isValid())
            {
                condition["value"] = c->value().toString();
            }
            conditions.append(condition);
        }

        std::vector<RuleAction>::const_iterator a = i->actions().begin();
        std::vector<RuleAction>::const_iterator aend = i->actions().end();

        QVariantList actions;

        for (; a != aend; ++a)
        {
            QVariantMap action;
            action["address"] = a->address();
            action["method"] = a->method();

            //parse body
            bool ok;
            QVariant body = Json::parse(a->body(), ok);

            if (ok)
            {
                action["body"] = body;
                actions.append(action);
            }
        }

        rule["name"] = i->name();
        rule["lasttriggered"] = i->lastTriggered();
        rule["created"] = i->creationtime();
        rule["timestriggered"] = i->timesTriggered();
        rule["owner"] = i->owner();
        rule["status"] = i->status();
        rule["conditions"] = conditions;
        rule["actions"] = actions;
        rule["periodic"] = (double)i->triggerPeriodic();

        QString etag = i->etag;
        etag.remove('"'); // no quotes allowed in string
        rule["etag"] = etag;

        rsp.map[i->id()] = rule;
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    return REQ_READY_SEND;
}

/*! Put all parameters in a map for later json serialization.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::ruleToMap(const Rule *rule, QVariantMap &map)
{
    if (!rule)
    {
        return false;
    }

    std::vector<RuleCondition>::const_iterator c = rule->conditions().begin();
    std::vector<RuleCondition>::const_iterator c_end = rule->conditions().end();

    QVariantList conditions;

    for (; c != c_end; ++c)
    {
        QVariantMap condition;
        condition["address"] = c->address();
        condition["operator"] = c->ooperator();
        if (c->value() != "" )
        {
            condition["value"] = c->value();
        }
        conditions.append(condition);
    }

    std::vector<RuleAction>::const_iterator a = rule->actions().begin();
    std::vector<RuleAction>::const_iterator a_end = rule->actions().end();

    QVariantList actions;

    for (; a != a_end; ++a)
    {
        QVariantMap action;
        action["address"] = a->address();
        action["method"] = a->method();

        //parse body
        bool ok;
        QVariant body = Json::parse(a->body(), ok);
        QVariantMap bodymap = body.toMap();

        QVariantMap::const_iterator b = bodymap.begin();
        QVariantMap::const_iterator b_end = bodymap.end();

        QVariantMap resultmap;

        for (; b != b_end; ++b)
        {
            resultmap[b.key()] = b.value();
        }

        action["body"] = resultmap;
        actions.append(action);
    }

    map["actions"] = actions;
    map["conditions"] = conditions;

    map["actions"] = actions;
    map["conditions"] = conditions;
    map["created"] = rule->creationtime();
    map["lasttriggered"] = rule->lastTriggered();
    map["name"] = rule->name();
    map["owner"] = rule->owner();
    map["periodic"] = rule->triggerPeriodic();
    map["status"] = rule->status();
    map["timestriggered"] = rule->timesTriggered();
    QString etag = rule->etag;
    etag.remove('"'); // no quotes allowed in string
    map["etag"] = etag;

    return true;
}


/*! GET /api/<apikey>/rules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getRule(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);

    if (req.path.size() != 4)
    {
        return -1;
    }

    const QString &id = req.path[3];

    Rule *rule = getRuleForId(id);

    if (!rule || (rule->state() == Rule::StateDeleted))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/rules/%1").arg(id), QString("resource, /rules/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    std::vector<RuleCondition>::const_iterator c = rule->conditions().begin();
    std::vector<RuleCondition>::const_iterator c_end = rule->conditions().end();

    QVariantList conditions;

    for (; c != c_end; ++c)
    {
        QVariantMap condition;
        condition["address"] = c->address();
        condition["operator"] = c->ooperator();
        if (c->value() != "" )
        {
            condition["value"] = c->value();
        }
        conditions.append(condition);
    }

    std::vector<RuleAction>::const_iterator a = rule->actions().begin();
    std::vector<RuleAction>::const_iterator a_end = rule->actions().end();

    QVariantList actions;

    for (; a != a_end; ++a)
    {
        QVariantMap action;
        action["address"] = a->address();
        action["method"] = a->method();

        //parse body
        bool ok;
        QVariant body = Json::parse(a->body(), ok);
        QVariantMap bodymap = body.toMap();

        QVariantMap::const_iterator b = bodymap.begin();
        QVariantMap::const_iterator b_end = bodymap.end();

        QVariantMap resultmap;

        for (; b != b_end; ++b)
        {
            resultmap[b.key()] = b.value();
        }

        action["body"] = resultmap;
        actions.append(action);
    }

    rsp.map["name"] = rule->name();
    rsp.map["lasttriggered"] = rule->lastTriggered();
    rsp.map["created"] = rule->creationtime();
    rsp.map["timestriggered"] = rule->timesTriggered();
    rsp.map["owner"] = rule->owner();
    rsp.map["status"] = rule->status();
    rsp.map["conditions"] = conditions;
    rsp.map["actions"] = actions;
    rsp.map["periodic"] = (double)rule->triggerPeriodic();

    QString etag = rule->etag;
    etag.remove('"'); // no quotes allowed in string
    rsp.map["etag"] = etag;

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/rules
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createRule(const ApiRequest &req, ApiResponse &rsp)
{
    bool error = false;

    rsp.httpStatus = HttpStatusOk;
    const QString &apikey = req.path[1];

    bool ok;
    Rule rule;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantList conditionsList = map["conditions"].toList();
    QVariantList actionsList = map["actions"].toList();

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/rules"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    userActivity();
/*
    if (rules.size() >= MAX_RULES_COUNT) //deletet rules will be count
    {
        rsp.list.append(errorToMap(ERR_RULE_ENGINE_FULL , QString("/rules/"), QString("The Rule Engine has reached its maximum capacity of %1 rules").arg(MAX_RULES_COUNT)));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
*/
    //check invalid parameter

    if (!map.contains("name"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/rules/name"), QString("invalid/missing parameters in body")));
    }

    if (conditionsList.size() < 1)
    {
        error = true;
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/rules/conditions"), QString("invalid/missing parameters in body")));
    }

    if (actionsList.size() < 1)
    {
        error = true;
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/rules/actions"), QString("invalid/missing parameters in body")));
    }

    if (conditionsList.size() > 8)
    {
        error = true;
        rsp.list.append(errorToMap(ERR_TOO_MANY_ITEMS, QString("/rules/conditions"), QString("too many items in list")));
    }

    if (actionsList.size() > 8)
    {
        error = true;
        rsp.list.append(errorToMap(ERR_TOO_MANY_ITEMS, QString("/rules/actions"), QString("too many items in list")));
    }

    if (map.contains("status")) // optional
    {
        QString status = map["status"].toString();
        if (!(status == "disabled" || status == "enabled"))
        {
            error = true;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/status"), QString("invalid value, %1, for parameter, status").arg(status)));
        }
    }

    if (map.contains("periodic")) // optional
    {
        int periodic = map["periodic"].toInt(&ok);

        if (!ok)
        {
            error = true;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/periodic"), QString("invalid value, %1, for parameter, peridoc").arg(map["periodic"].toString())));
        }
        else
        {
            rule.setTriggerPeriodic(periodic);
        }
    }

    //resolve errors
    if (error)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
    else
    {
        QString name = map["name"].toString();

        if ((map["name"].type() == QVariant::String) && !name.isEmpty())
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;

            // create a new rule id
            rule.setId("1");

            do {
                ok = true;
                std::vector<Rule>::const_iterator i = rules.begin();
                std::vector<Rule>::const_iterator end = rules.end();

                for (; i != end; ++i)
                {
                    if (i->id() == rule.id())
                    {
                        rule.setId(QString::number(i->id().toInt() + 1));
                        ok = false;
                    }
                }
            } while (!ok);

            //setName
            rule.setName(name);
            rule.setOwner(apikey);
            rule.setCreationtime(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss"));

            //setStatus optional
            if (map.contains("status"))
            {
                rule.setStatus(map["status"].toString());
            }

            //setActions
            if (checkActions(actionsList, rsp))
            {
                std::vector<RuleAction> actions;
                QVariantList::const_iterator ai = actionsList.begin();
                QVariantList::const_iterator aend = actionsList.end();

                for (; ai != aend; ++ai)
                {
                    QVariantMap bodymap = (ai->toMap()["body"]).toMap();
                    RuleAction newAction;
                    newAction.setAddress(ai->toMap()["address"].toString());
                    newAction.setBody(Json::serialize(bodymap));
                    newAction.setMethod(ai->toMap()["method"].toString());
                    actions.push_back(newAction);
                }

                rule.setActions(actions);
            }
            else
            {
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }

            //setConditions
            if (checkConditions(conditionsList, rsp))
            {
                std::vector<RuleCondition> conditions;
                QVariantList::const_iterator ci = conditionsList.begin();
                QVariantList::const_iterator cend = conditionsList.end();

                for (; ci != cend; ++ci)
                {
                    RuleCondition cond(ci->toMap());
                    conditions.push_back(cond);
                }

                rule.setConditions(conditions);
            }
            else
            {
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }

            updateEtag(rule.etag);
            updateEtag(gwConfigEtag);

            {
                bool found = false;
                std::vector<Rule>::iterator ri = rules.begin();
                std::vector<Rule>::iterator rend = rules.end();
                for (; ri != rend; ++ri)
                {
                    if (ri->actions() == rule.actions() &&
                        ri->conditions() == rule.conditions())
                    {
                        DBG_Printf(DBG_INFO, "replace existing rule with newly created one\n");
                        found = true;
                        *ri = rule;
                        queueCheckRuleBindings(*ri);
                        break;
                    }
                }

                if (!found)
                {
                    rules.push_back(rule);
                    queueCheckRuleBindings(rule);
                }
            }
            queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);

            rspItemState["id"] = rule.id();
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;
            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/rules"), QString("body contains invalid JSON")));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }

    return REQ_READY_SEND;
}


/*! PUT /api/<apikey>/rules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateRule(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    bool error = false;
    bool changed = false;

    QString id = req.path[3];

    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantList conditionsList;
    QVariantList actionsList;

    QString name;
    QString status;
    int periodic = 0;

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/rules"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    userActivity();

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        if(!((pi.key() == QLatin1String("name")) || (pi.key() == QLatin1String("status")) || (pi.key() == QLatin1String("actions")) || (pi.key() == QLatin1String("conditions")) || (pi.key() == QLatin1String("periodic"))))
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/rules/%1/%2").arg(id).arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (map.contains("name")) // optional
    {
        name = map["name"].toString();

        if ((map["name"].type() == QVariant::String) && !(name.isEmpty()))
        {
            if (name.size() > MAX_RULE_NAME_LENGTH)
            {
                error = true;
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/%1/name").arg(id), QString("invalid value, %1, for parameter, /rules/%2/name").arg(name).arg(id)));
                rsp.httpStatus = HttpStatusBadRequest;
                name = QString();
            }
        }
        else
        {
            error = true;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/%1/name").arg(id), QString("invalid value, %1, for parameter, /rules/%2/name").arg(name).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
            name = QString();
        }
    }

    if (map.contains("conditions")) //optional
    {
        conditionsList = map["conditions"].toList();
        if (conditionsList.size() < 1)
        {
            error = true;
            rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/rules/conditions"), QString("invalid/missing parameters in body")));
        }

        if (conditionsList.size() > 8)
        {
            error = true;
            rsp.list.append(errorToMap(ERR_TOO_MANY_ITEMS, QString("/rules/conditions"), QString("too many items in list")));
        }
    }

    if (map.contains("actions")) //optional
    {
        actionsList = map["actions"].toList();
        if (actionsList.size() < 1)
        {
            error = true;
            rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/rules/actions"), QString("invalid/missing parameters in body")));
        }

        if (actionsList.size() > 8)
        {
            error = true;
            rsp.list.append(errorToMap(ERR_TOO_MANY_ITEMS, QString("/rules/actions"), QString("too many items in list")));
        }
    }
    if (map.contains("status")) // optional
    {
        status = map["status"].toString();
        if (!(status == "disabled" || status == "enabled"))
        {
            error = true;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/status"), QString("invalid value, %1, for parameter, status").arg(status)));
        }
    }

    if (map.contains("periodic")) // optional
    {
        periodic = map["periodic"].toInt(&ok);

        if (!ok)
        {
            error = true;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/periodic"), QString("invalid value, %1, for parameter, peridoc").arg(map["periodic"].toString())));
        }
    }

    //resolve errors
    if (error)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    std::vector<Rule>::iterator i = rules.begin();
    std::vector<Rule>::iterator end = rules.end();

    for (; i != end; ++i)
    {
        if (i->state() != Rule::StateNormal)
        {
            continue;
        }

        if (i->id() == id)
        {
            // first delete old binding if present then create new binding with updated rule
            if (map.contains("actions") || map.contains("conditions"))
            {
                i->setStatus("disabled");
                queueCheckRuleBindings(*i);
            }

            //setName optional
            if (!name.isEmpty())
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/rules/%1/name").arg(id)] = name;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                if (i->name() != name)
                {
                    changed = true;
                    i->setName(name);
                }
            }

            //setStatus optional
            if (map.contains("status"))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/rules/%1/status").arg(id)] = status;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                if (i->status() != status)
                {
                    changed = true;
                    i->setStatus(status);
                }
            }

            // periodic optional
            if (map.contains("periodic"))
            {
                if (i->triggerPeriodic() != periodic)
                {
                    changed = true;
                    i->setTriggerPeriodic(periodic);
                }
            }

            //setActions optional
            if (map.contains("actions"))
            {
                changed = true;
                if (checkActions(actionsList,rsp))
                {
                    std::vector<RuleAction> actions;
                    QVariantList::const_iterator ai = actionsList.begin();
                    QVariantList::const_iterator aend = actionsList.end();

                    for (; ai != aend; ++ai)
                    {
                        RuleAction newAction;
                        newAction.setAddress(ai->toMap()["address"].toString());
                        newAction.setBody(Json::serialize(ai->toMap()["body"].toMap()));
                        newAction.setMethod(ai->toMap()["method"].toString());
                        actions.push_back(newAction);
                    }
                    i->setActions(actions);

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/rules/%1/actions").arg(id)] = actionsList;
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            //setConditions optional
            if (map.contains("conditions"))
            {
                changed = true;
                if (checkConditions(conditionsList, rsp))
                {
                    std::vector<RuleCondition> conditions;
                    QVariantList::const_iterator ci = conditionsList.begin();
                    QVariantList::const_iterator cend = conditionsList.end();

                    for (; ci != cend; ++ci)
                    {
                        RuleCondition newCondition;
                        newCondition.setAddress(ci->toMap()["address"].toString());
                        newCondition.setOperator(ci->toMap()["operator"].toString());
                        newCondition.setValue(ci->toMap()["value"].toString());
                        conditions.push_back(newCondition);

                    }
                    i->setConditions(conditions);

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/rules/%1/conditions").arg(id)] = conditionsList;
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            if (!map.contains("status"))
            {
                i->setStatus("enabled");
            }
            DBG_Printf(DBG_INFO, "force verify of rule %s: %s\n", qPrintable(i->id()), qPrintable(i->name()));
            i->lastVerify = 0;
            if (!verifyRulesTimer->isActive())
            {
                verifyRulesTimer->start(500);
            }

            if (changed)
            {
                updateEtag(i->etag);
                updateEtag(gwConfigEtag);
                queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);
            }

            break;
        }
    }

    return REQ_READY_SEND;
}


/*! Validate rule actions.
    \param actionsList the actionsList
 */
bool DeRestPluginPrivate::checkActions(QVariantList actionsList, ApiResponse &rsp)
{
    QVariantList::const_iterator ai = actionsList.begin();
    QVariantList::const_iterator aend = actionsList.end();

    for (; ai != aend; ++ai)
    {
        QString address = ai->toMap()["address"].toString();
        QString method = ai->toMap()["method"].toString();
        QString body = ai->toMap()["body"].toString();

        QStringList addrList = ai->toMap()["address"].toString().split('/', QString::SkipEmptyParts);

        //check addresses
        //address must begin with / and a valid resource
        // /<ressouce>/<id>
        // /groups/7/action
        // /lights/1/state
        // /schedules/5
        // /sensors/2

        if (addrList.size() < 2)
        {
            rsp.list.append(errorToMap(ERR_ACTION_ERROR, QString(address),
                            QString("Rule actions contain errors or an action on a unsupported resource")));
            return false;
        }

        //no dublicate addresses allowed
        const char *resources[] = { "groups", "lights", "schedules", "sensors", 0 };

        for (int i = 0; ; i++)
        {
            if (!resources[i])
            {
                rsp.list.append(errorToMap(ERR_ACTION_ERROR, QString(address),
                                QString("Rule actions contain errors or an action on a unsupported resource")));
                return false;
            }

            if (addrList[0] == resources[i])
            {
                break; // supported
            }
        }

        //check methods
        if(!((method == QLatin1String("PUT")) || (method == QLatin1String("POST")) || (method == QLatin1String("DELETE")) || (method == QLatin1String("BIND"))))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE , QString("rules/method"), QString("invalid value, %1, for parameter, method").arg(method)));
            return false;
        }

        //check body
        bool ok;
        Json::parse(body, ok);
        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/rules/"), QString("body contains invalid JSON")));
            return false;
        }
    }

    return true;
}

/*! Rule conditions contain errors or operator combination is not allowed.
    \param conditionsList the conditionsList
 */
bool DeRestPluginPrivate::checkConditions(QVariantList conditionsList, ApiResponse &rsp)
{
    // check condition parameters
    QVariantList::const_iterator ci = conditionsList.begin();
    QVariantList::const_iterator cend = conditionsList.end();

    for (; ci != cend; ++ci)
    {
        QVariantMap condition = ci->toMap();
        QString address = condition["address"].toString();
        QString op = condition["operator"].toString();

        QStringList addrList = address.split('/', QString::SkipEmptyParts);

        Sensor *sensor = (addrList.size() > 3) ? getSensorNodeForId(addrList[1]) : 0;

        if (!sensor || address.isEmpty() || op.isEmpty())
        {
            rsp.list.append(errorToMap(ERR_CONDITION_ERROR, QString(address),
                QString("Condition error")));
            return false;
        }

        const SensorResourceDescriptor *rd = resourceDescriptors;

        while (rd->sensorType)
        {
            if ((rd->sensorType[0] == '0' || // any
                 rd->sensorType == sensor->type()) &&
                address.contains(rd->resource) &&
                strstr(rd->operators, qPrintable(op)))
            {
                break; // match
            }
            rd++;
        }

        if (!rd)
        {
            rsp.list.append(errorToMap(ERR_CONDITION_ERROR, QString(address),
                QString("Condition error")));
            return false;
        }


        bool ok = false;
        if      (rd->type == RTYPE_INT && condition["value"].type() == QVariant::Double) { ok = true; }
        else if (rd->type == RTYPE_BOOL && condition["value"].type() == QVariant::Bool) { ok = true; }
        else if (rd->type == RTYPE_NONE && !condition.contains("value")) { ok = true; }

        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_CONDITION_ERROR, QString(address),
                QString("Condition error")));
            return false;
        }
    }
    return true;
}

/*! Trigger rules based on events.
    \param event the event to process
 */
void DeRestPluginPrivate::handleRuleEvent(const Event &event)
{
    std::vector<Rule>::iterator r = rules.begin();
    std::vector<Rule>::iterator rend = rules.end();

    for (; r != rend; ++r)
    {
        std::vector<RuleCondition>::const_iterator c = r->conditions().begin();
        std::vector<RuleCondition>::const_iterator cend = r->conditions().end();

        bool ok = !r->conditions().empty();

        for (; ok && c != cend; ++c)
        {
            // check prefix (sensors, lights, ...)
            if (!c->address().startsWith(event.resource()))
            {
                ok = false;
                break;
            }

            // check suffix (state/buttonevent, ...)
            if (!c->address().endsWith(event.what()))
            {
                ok = false;
                break;
            }

            if (!event.id().isEmpty())
            {
                // check id
                if (event.id() != c->id())
                {
                    ok = false;
                    break;
                }
            }

            if (c->op() == RuleCondition::OpEqual)
            {
                if (c->numericValue() != event.numericValue()) { ok = false; break; }
            }
            else if (c->op() == RuleCondition::OpGreaterThan)
            {
                if (c->numericValue() < event.numericValue()) { ok = false; break; }
            }
            else if (c->op() == RuleCondition::OpLowerThan)
            {
                if (c->numericValue() > event.numericValue()) { ok = false; break; }
            }
            else if (c->op() == RuleCondition::OpDx)
            {
                // TODO
            }
        }

        if (ok)
        {
            triggerRule(*r);
        }
    }
}

/*! DELETE /api/<apikey>/rules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteRule(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Rule *rule = getRuleForId(id);

    userActivity();

    if (!rule || (rule->state() == Rule::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/rules/%1").arg(id), QString("resource, /rules/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    rule->setState(Rule::StateDeleted);
    rule->setStatus("disabled");
    queueCheckRuleBindings(*rule);

    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["id"] = id;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    updateEtag(gwConfigEtag);
    updateEtag(rule->etag);

    queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! Add a binding task to the queue and prevent double entries.
    \param bindingTask - the binding task
 */
void DeRestPluginPrivate::queueBindingTask(const BindingTask &bindingTask)
{
    if (!apsCtrl || apsCtrl->networkState() != deCONZ::InNetwork)
    {
        return;
    }

    const std::list<BindingTask>::const_iterator i = std::find(bindingQueue.begin(), bindingQueue.end(), bindingTask);

    if (i == bindingQueue.end())
    {
        DBG_Printf(DBG_INFO_L2, "queue binding task for 0x%016llu, cluster 0x%04X\n", bindingTask.binding.srcAddress, bindingTask.binding.clusterId);
        bindingQueue.push_back(bindingTask);
    }
    else
    {
        DBG_Printf(DBG_INFO, "discard double entry in binding queue\n");
    }
}

/*! Starts verification that the ZigBee bindings of a rule are present
    on the source device.
    \param rule the rule to verify
 */
void DeRestPluginPrivate::queueCheckRuleBindings(const Rule &rule)
{
    quint64 srcAddress = 0;
    quint8 srcEndpoint = 0;
    BindingTask bindingTask;
    bindingTask.state = BindingTask::StateCheck;
    Sensor *sensorNode = 0;

    Q_Q(DeRestPlugin);
    if (!q->pluginActive())
    {
        return;
    }

    if (rule.state() == Rule::StateNormal && rule.status() == QLatin1String("enabled"))
    {
        bindingTask.action = BindingTask::ActionBind;
    }
    else if (rule.state() == Rule::StateDeleted || rule.status() == QLatin1String("disabled"))
    {
        bindingTask.action = BindingTask::ActionUnbind;
    }
    else
    {
        DBG_Printf(DBG_INFO, "ignored checking of rule %s\n", qPrintable(rule.name()));
        return;
    }

    {   // search in conditions for binding srcAddress and srcEndpoint

        std::vector<RuleCondition>::const_iterator i = rule.conditions().begin();
        std::vector<RuleCondition>::const_iterator end = rule.conditions().end();

        for (; i != end; ++i)
        {
            // operator equal used to refer to srcEndpoint
            if (i->ooperator() != QLatin1String("eq"))
            {
                continue;
            }

            QStringList srcAddressLs = i->address().split('/', QString::SkipEmptyParts);

            if (srcAddressLs.size() != 4) // /sensors/<id>/state/(buttonevent|illuminance)
            {
                continue;
            }

            if (srcAddressLs[0] != QLatin1String("sensors"))
            {
                continue;
            }

            if (srcAddressLs[2] != QLatin1String("state"))
            {
                continue;
            }

            if ((srcAddressLs[3] == QLatin1String("buttonevent")) ||
                (srcAddressLs[3] == QLatin1String("illuminance")) ||
                (srcAddressLs[3] == QLatin1String("presence")))
            {
                sensorNode = getSensorNodeForId(srcAddressLs[1]);

                if (sensorNode && sensorNode->isAvailable() && sensorNode->node())
                {
                    bool ok = false;
                    quint16 ep = i->value().toUInt(&ok);

                    if (ok)
                    {
                        const std::vector<quint8> &activeEndpoints = sensorNode->node()->endpoints();

                        for (uint i = 0; i < activeEndpoints.size(); i++)
                        {
                            // check valid endpoint in 'value'
                            if (ep == activeEndpoints[i])
                            {
                                srcAddress = sensorNode->address().ext();
                                srcEndpoint = ep;
                                if (!sensorNode->mustRead(READ_BINDING_TABLE))
                                {
                                    sensorNode->enableRead(READ_BINDING_TABLE);
                                    sensorNode->setNextReadTime(READ_BINDING_TABLE, QTime::currentTime());
                                }
                                q->startZclAttributeTimer(1000);
                                break;
                            }
                        }

                        // found source addressing?
                        if ((srcAddress == 0) || (srcEndpoint == 0))
                        {
                            DBG_Printf(DBG_INFO, "no src addressing found for rule %s\n", qPrintable(rule.name()));
                        }
                    }
                }
                else
                {
                    void *n = 0;
                    uint avail = false;

                    if (sensorNode)
                    {
                        avail = sensorNode->isAvailable();
                        n = sensorNode->node();
                    }

                    DBG_Printf(DBG_INFO, "skip verify rule %s for sensor %s (available = %u, node = %p, sensorNode = %p)\n",
                               qPrintable(rule.name()), qPrintable(srcAddressLs[1]), avail, n, sensorNode);
                }
            }
        }
    }

    if (!sensorNode)
    {
        return;
    }


    // found source addressing?
    if ((srcAddress == 0) || (srcEndpoint == 0))
    {
        return;
    }

    bindingTask.restNode = sensorNode;

    DBG_Printf(DBG_INFO, "verify Rule %s: %s\n", qPrintable(rule.id()), qPrintable(rule.name()));

    { // search in actions for binding dstAddress, dstEndpoint and clusterId
        std::vector<RuleAction>::const_iterator i = rule.actions().begin();
        std::vector<RuleAction>::const_iterator end = rule.actions().end();

        for (; i != end; ++i)
        {
            if (i->method() != QLatin1String("BIND"))
            {
                continue;
            }

            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = srcAddress;
            bnd.srcEndpoint = srcEndpoint;
            bool ok = false;

            if (!sensorNode->config().on())
            {
                if (bindingTask.action == BindingTask::ActionBind)
                {
                    DBG_Printf(DBG_INFO, "Sensor %s is 'off', prevent Rule %s: %s activation\n", qPrintable(sensorNode->id()), qPrintable(rule.id()), qPrintable(rule.name()));
                    bindingTask.action = BindingTask::ActionUnbind;
                }
            }

            QStringList dstAddressLs = i->address().split('/', QString::SkipEmptyParts);

            // /groups/0/action
            // /lights/2/state
            if (dstAddressLs.size() == 3)
            {
                if (dstAddressLs[0] == QLatin1String("groups"))
                {
                    bnd.dstAddress.group = dstAddressLs[1].toUShort(&ok);
                    bnd.dstAddrMode = deCONZ::ApsGroupAddress;
                }
                else if (dstAddressLs[0] == QLatin1String("lights"))
                {
                    LightNode *lightNode = getLightNodeForId(dstAddressLs[1]);
                    if (lightNode)
                    {
                        bnd.dstAddress.ext = lightNode->address().ext();
                        bnd.dstEndpoint = lightNode->haEndpoint().endpoint();
                        bnd.dstAddrMode = deCONZ::ApsExtAddress;
                        ok = true;
                    }
                }
                else
                {
                    // unsupported addressing
                    continue;
                }

                if (!ok)
                {
                    continue;
                }

                // action.body might contain multiple 'bindings'
                // TODO check if clusterId is available (finger print?)

                if (i->body().contains(QLatin1String("on")))
                {
                    bnd.clusterId = ONOFF_CLUSTER_ID;
                    queueBindingTask(bindingTask);
                }

                if (i->body().contains(QLatin1String("bri")))
                {
                    bnd.clusterId = LEVEL_CLUSTER_ID;
                    queueBindingTask(bindingTask);
                }

                if (i->body().contains(QLatin1String("scene")))
                {
                    bnd.clusterId = SCENE_CLUSTER_ID;
                    queueBindingTask(bindingTask);
                }

                if (i->body().contains(QLatin1String("illum")))
                {
                    bnd.clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID;
                    queueBindingTask(bindingTask);
                }

                if (i->body().contains(QLatin1String("occ")))
                {
                    bnd.clusterId = OCCUPANCY_SENSING_CLUSTER_ID;
                    queueBindingTask(bindingTask);
                }
            }
        }
    }

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }
}

/*! Triggers actions of a rule if needed.
    \param rule - the rule to check
 */
void DeRestPluginPrivate::triggerRuleIfNeeded(Rule &rule)
{
    if (!apsCtrl || (apsCtrl->networkState() != deCONZ::InNetwork))
    {
        return;
    }

    if (!(rule.state() == Rule::StateNormal && rule.status() == QLatin1String("enabled")))
    {
        return;
    }

    if (rule.triggerPeriodic() < 0)
    {
        return;
    }

    if (rule.triggerPeriodic() == 0)
    {
        // trigger on event
        return;
    }

    if (rule.triggerPeriodic() > 0)
    {
        if (rule.lastTriggeredTime().isValid() &&
            rule.lastTriggeredTime().elapsed() < rule.triggerPeriodic())
        {
            // not yet time
            return;
        }
    }

    std::vector<RuleCondition>::const_iterator ci = rule.conditions().begin();
    std::vector<RuleCondition>::const_iterator cend = rule.conditions().end();

    for (; ci != cend; ++ci)
    {
        if (!ci->address().startsWith(QLatin1String("/sensors")))
        {
            return;
        }

        Sensor *sensor = getSensorNodeForId(ci->id());

        if (!sensor)
        {
            return;
        }

        if (!sensor->isAvailable())
        {
            return;
        }

        if (ci->address().endsWith(QLatin1String("buttonevent")))
        {
            return; // TODO
        }
        else if (ci->address().endsWith(QLatin1String("illuminance")))
        {
            { // check if value is fresh enough
                NodeValue &val = sensor->getZclValue(ILLUMINANCE_MEASUREMENT_CLUSTER_ID, 0x0000);

                if (!val.timestamp.isValid() ||
                     val.timestamp.elapsed() > MAX_RULE_ILLUMINANCE_VALUE_AGE_MS)
                {
                    if (val.timestampLastReadRequest.isValid() &&
                        val.timestampLastReadRequest.elapsed() < (MAX_RULE_ILLUMINANCE_VALUE_AGE_MS / 2))
                    {
                        return;
                    }

                    std::vector<quint16> attrs;
                    attrs.push_back(0x0000); // measured value
                    DBG_Printf(DBG_INFO, "force read illuminance value of 0x%016llX\n", sensor->address().ext());
                    if (readAttributes(sensor, sensor->fingerPrint().endpoint, ILLUMINANCE_MEASUREMENT_CLUSTER_ID, attrs))
                    {
                        val.timestampLastReadRequest.start();
                    }

                    return;
                }
            }

            uint cval = ci->numericValue();

            if (ci->op() == RuleCondition::OpLowerThan)
            {
                if (sensor->state().lux() >= cval)
                {
                    return; // condition not met
                }
            }
            else if (ci->op() == RuleCondition::OpGreaterThan)
            {
                if (sensor->state().lux() <= cval)
                {
                    return; // condition not met
                }
            }
            else
            {
                return; // unsupported condition operator
            }
        }
        else
        {
            return; // unsupported condition address
        }
    }

    triggerRule(rule);
}

/*! Triggers actions of a rule.
    \param rule - the rule to trigger
 */
void DeRestPluginPrivate::triggerRule(Rule &rule)
{
    if (rule.state() != Rule::StateNormal || !rule.isEnabled())
    {
        return;
    }

    DBG_Printf(DBG_INFO, "trigger rule %s - %s\n", qPrintable(rule.id()), qPrintable(rule.name()));

    bool triggered = false;
    std::vector<RuleAction>::const_iterator ai = rule.actions().begin();
    std::vector<RuleAction>::const_iterator aend = rule.actions().end();

    for (; ai != aend; ++ai)
    {
        if (ai->method() != QLatin1String("PUT"))
            return;

        QStringList path = ai->address().split(QChar('/'), QString::SkipEmptyParts);

        if (path.size() < 3) // groups, <id>, action
            return;

        QHttpRequestHeader hdr(ai->method(), ai->address());

        // paths start with /api/<apikey/ ...>
        path.prepend(rule.owner()); // apikey
        path.prepend(QLatin1String("api")); // api

        ApiRequest req(hdr, path, NULL, ai->body());
        ApiResponse rsp; // dummy

        // todo, dispatch request function
        if (path[2] == QLatin1String("groups"))
        {
            if (handleGroupsApi(req, rsp) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
        }
        else if (path[2] == QLatin1String("lights"))
        {
            if (handleLightsApi(req, rsp) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
        }
        else
        {
            DBG_Printf(DBG_INFO, "unsupported rule action address %s\n", qPrintable(ai->address()));
            return;
        }
    }

    if (triggered)
    {
        rule.setLastTriggered(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss"));
        rule.setTimesTriggered(rule.timesTriggered() + 1);
    }
}

/*! Verifies that rule bindings are valid. */
void DeRestPluginPrivate::verifyRuleBindingsTimerFired()
{
    if (!apsCtrl || (apsCtrl->networkState() != deCONZ::InNetwork) || rules.empty())
    {
        return;
    }

    Q_Q(DeRestPlugin);
    if (!q->pluginActive())
    {
        return;
    }

    if (verifyRuleIter >= rules.size())
    {
        verifyRuleIter = 0;
    }

    Rule &rule = rules[verifyRuleIter];

    triggerRuleIfNeeded(rule);

    if (bindingQueue.size() < 16)
    {
        if (rule.state() == Rule::StateNormal)
        {
            if ((rule.lastVerify + Rule::MaxVerifyDelay) < idleTotalCounter)
            {
                rule.lastVerify = idleTotalCounter;
                queueCheckRuleBindings(rule);
            }
        }
    }
    else
    {
        DBG_Printf(DBG_INFO, "");
    }

    verifyRuleIter++;
}
