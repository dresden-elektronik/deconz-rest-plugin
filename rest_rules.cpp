/*
 * Copyright (c) 2016-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QBuffer>
#include <QString>
#include <QVariantMap>
#include <QRegExp>
#include <QStringBuilder>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

#define MAX_RULES_COUNT 500
#define FAST_RULE_CHECK_INTERVAL_MS 10
#define NORMAL_RULE_CHECK_INTERVAL_MS 100

/*! Rules REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleRulesApi(const ApiRequest &req, ApiResponse &rsp)
{
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
    // PUT, PATCH /api/<apikey>/rules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[2] == "rules"))
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
    Q_UNUSED(req)
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
        if (i->lastTriggered().isValid())
        {
            rule["lasttriggered"] = i->lastTriggered().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
        }
        else
        {
            rule["lasttriggered"] = QLatin1String("none");
        }
        rule["created"] = i->creationtime();
        rule["timestriggered"] = i->timesTriggered();
        rule["owner"] = i->owner();
        rule["status"] = i->status();
        rule["conditions"] = conditions;
        rule["actions"] = actions;
        rule["periodic"] = static_cast<double>(i->triggerPeriodic());

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
        if (c->value().isValid())
        {
            condition["value"] = c->value().toString();
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
    if (rule->lastTriggered().isValid())
    {
        map["lasttriggered"] = rule->lastTriggered().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
    }
    else
    {
        map["lasttriggered"] = QLatin1String("none");
    }
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
        if (c->value().isValid())
        {
            condition["value"] = c->value().toString();
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
    if (rule->lastTriggered().isValid())
    {
        rsp.map["lasttriggered"] = rule->lastTriggered().toString("yyyy-MM-ddTHH:mm:ss");
    }
    else
    {
        rsp.map["lasttriggered"] = QLatin1String("none");
    }
    rsp.map["created"] = rule->creationtime();
    rsp.map["timestriggered"] = rule->timesTriggered();
    rsp.map["owner"] = rule->owner();
    rsp.map["status"] = rule->status();
    rsp.map["conditions"] = conditions;
    rsp.map["actions"] = actions;
    rsp.map["periodic"] = static_cast<double>(rule->triggerPeriodic());

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
            if (name.size() > MAX_RULE_NAME_LENGTH)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/rules/name"), QString("invalid/missing parameters in body")));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
            
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
                    RuleAction newAction;
                    const QVariantMap m = ai->toMap();
                    newAction.setAddress(m["address"].toString());
                    newAction.setBody(Json::serialize(m["body"].toMap()));
                    newAction.setMethod(m["method"].toString());
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
                    const RuleCondition cond(ci->toMap());
                    if (cond.isValid())
                    {
                        conditions.push_back(cond);
                    }
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

            DBG_Printf(DBG_INFO, "create rule %s: %s\n", qPrintable(rule.id()), qPrintable(rule.name()));
            rules.push_back(rule);
            queueCheckRuleBindings(rule);
            indexRulesTriggers();
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


/*! PUT, PATCH /api/<apikey>/rules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateRule(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    bool error = false;
    bool changed = false;

    QString id = req.path[3];

    Rule *rule = getRuleForId(id);

    if (!rule || (rule->state() == Rule::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/rules/%1").arg(id), QString("resource, /rules/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    DBG_Printf(DBG_INFO, "update rule %s: %s\n", qPrintable(id), qPrintable(rule->name()));

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

    // first delete old binding if present then create new binding with updated rule
    if (map.contains("actions") || map.contains("conditions"))
    {
        rule->setStatus("disabled");
        queueCheckRuleBindings(*rule);
    }

    //setName optional
    if (!name.isEmpty())
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[QString("/rules/%1/name").arg(id)] = name;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        if (rule->name() != name)
        {
            changed = true;
            rule->setName(name);
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
        if (rule->status() != status)
        {
            changed = true;
            rule->setStatus(status);
        }
    }

    // periodic optional
    if (map.contains("periodic"))
    {
        if (rule->triggerPeriodic() != periodic)
        {
            changed = true;
            rule->setTriggerPeriodic(periodic);
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
            rule->setActions(actions);

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
                const RuleCondition cond(ci->toMap());
                if (cond.isValid())
                {
                    conditions.push_back(cond);
                }
            }
            rule->setConditions(conditions);

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/rules/%1/conditions").arg(id)] = conditionsList;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            indexRulesTriggers();
        }
        else
        {
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (!map.contains("status"))
    {
        rule->setStatus("enabled");
    }
    DBG_Printf(DBG_INFO_L2, "force verify of rule %s: %s\n", qPrintable(rule->id()), qPrintable(rule->name()));
    rule->lastBindingVerify = 0;

    if (changed)
    {
        updateEtag(rule->etag);
        updateEtag(gwConfigEtag);
        queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);
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
        const char *resources[] = { "groups", "lights", "schedules", "scenes", "sensors", "rules", nullptr };

        for (int i = 0; ; i++)
        {
            if (address.startsWith(QLatin1String("http"))) // webhook http/https
            {
                break; // supported
            }

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
        if(!(method == QLatin1String("PUT") || method == QLatin1String("POST") || method == QLatin1String("DELETE") || method == QLatin1String("BIND") || method == QLatin1String("GET")))
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
        const RuleCondition cond(ci->toMap());

        Resource *resource = cond.isValid() ? getResource(cond.resource(), cond.id()) : nullptr;
        ResourceItem *item = resource ? resource->item(cond.suffix()) : nullptr;

        if (!resource || !item)
        {
            rsp.list.append(errorToMap(ERR_CONDITION_ERROR, QString(cond.address()), QLatin1String("Condition error")));
            return false;
        }
    }
    return true;
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

    DBG_Printf(DBG_INFO, "delete rule %s: %s\n", qPrintable(id), qPrintable(rule->name()));

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
    Sensor *sensorNode = nullptr;

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
            if (i->op() != RuleCondition::OpEqual)
            {
                continue;
            }

            if (i->resource() != RSensors)
            {
                continue;
            }

            if ((i->suffix() == RStateButtonEvent) ||
                (i->suffix() == RStateLightLevel) || // TODO check webapp2 change illuminance --> lightlevel
                (i->suffix() == RStatePresence))
            {
                sensorNode = getSensorNodeForId(i->id());

                if (sensorNode && sensorNode->isAvailable() && sensorNode->node())
                {

                    if (!sensorNode->modelId().startsWith(QLatin1String("FLS-NB")))
                    {
                        // whitelist binding support
                        return;
                    }

                    bool ok = false;
                    uint ep = i->value().toUInt(&ok);

                    if (ok && ep <= 255)
                    {
                        const std::vector<quint8> &activeEndpoints = sensorNode->node()->endpoints();

                        for (uint i = 0; i < activeEndpoints.size(); i++)
                        {
                            // check valid endpoint in 'value'
                            if (ep == activeEndpoints[i])
                            {
                                srcAddress = sensorNode->address().ext();
                                srcEndpoint = static_cast<quint8>(ep);
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
                    void *n = nullptr;
                    uint avail = false;

                    if (sensorNode)
                    {
                        avail = sensorNode->isAvailable();
                        n = sensorNode->node();
                    }

                    DBG_Printf(DBG_INFO_L2, "skip verify rule %s for sensor %s (available = %u, node = %p, sensorNode = %p)\n",
                               qPrintable(rule.name()), qPrintable(i->id()), avail, n, sensorNode);
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

            if (!sensorNode->toBool(RConfigOn))
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

/*! Evaluates rule.
    \param rule - the rule to check
    \param e - the trigger event
    \param eResource - the event resource
    \param eItem - the event resource item
    \param now - the current date/time to check the rule against
    \return true if rule can be triggered
 */
bool DeRestPluginPrivate::evaluateRule(Rule &rule, const Event &e, Resource *eResource, ResourceItem *eItem, QDateTime now, QDateTime previousNow)
{
    if (!apsCtrl || !eItem || !eResource || (apsCtrl->networkState() != deCONZ::InNetwork))
    {
        return false;
    }

    if (rule.state() != Rule::StateNormal || !rule.isEnabled())
    {
        return false;
    }

    if (rule.triggerPeriodic() < 0)
    {
        return false;
    }

    if (rule.triggerPeriodic() > 0)
    {
        if (rule.lastTriggered().isValid() &&
            rule.lastTriggered().addMSecs(rule.triggerPeriodic()) > now)
        {
            // not yet time
            return false;
        }
    }

    std::vector<RuleCondition>::const_iterator c = rule.conditions().begin();
    std::vector<RuleCondition>::const_iterator cend = rule.conditions().end();

    for (; c != cend; ++c)
    {
        Resource *resource = getResource(c->resource(), c->id());
        ResourceItem *item = resource ? resource->item(c->suffix()) : nullptr;

        // the condition value might refer to another resource
        Resource *valueResource = c->valueResource() ? getResource(c->valueResource(), c->valueId()) : nullptr;
        ResourceItem *valueItem = valueResource ? valueResource->item(c->valueSuffix()) : nullptr;

        if (!resource || !item)
        {
            DBG_Printf(DBG_INFO, "rule: %s, resource %s : %s id: %s (cond: %s) not found\n",
                       qPrintable(rule.id()), c->resource(), c->suffix(),
                       qPrintable(c->id()), qPrintable(c->address()));

            if (!resource)
            {
                DBG_Printf(DBG_INFO, "\tdisable rule %s: %s\n", qPrintable(rule.id()), qPrintable(rule.name()));
                rule.setStatus(QLatin1String("disabled"));
            }
            return false;
        }

        if (!item->lastSet().isValid()) { return false; }

        if (resource->prefix() == RSensors)
        {
            // don't trigger rule if sensor is disabled
            ResourceItem *item2 = resource->item(RConfigOn);
            if (item2 && !item2->toBool())
            {
                return false;
            }
        }

        if (c->op() == RuleCondition::OpEqual)
        {
            if (c->numericValue() != item->toNumber())
            {
                return false;
            }

            if (item == eItem && e.num() == e.numPrevious())
            {
                return false; // item was not changed
            }
        }
        else if (c->op() == RuleCondition::OpNotEqual)
        {
            if (c->numericValue() == item->toNumber())
            {
                return false;
            }

            if (item == eItem && e.num() == e.numPrevious())
            {
                return false; // item was not changed
            }
        }
        else if (c->op() == RuleCondition::OpGreaterThan && item->descriptor().suffix == RStateLocaltime)
        {
            if (valueItem && valueItem->descriptor().suffix == RStateLocaltime)
            {
                if (valueItem->toNumber() < item->toNumber())
                {
                    return false;
                }
            }
            else if (valueItem && valueItem->descriptor().suffix == RConfigLocalTime)
            {
                const QDateTime t1 = QDateTime::fromMSecsSinceEpoch(item->toNumber());
                if (now.time() < t1.time())
                {
                    return false;
                }
            }
        }
        else if (c->op() == RuleCondition::OpLowerThan && item->descriptor().suffix == RStateLocaltime)
        {
            if (valueItem && valueItem->descriptor().suffix == RStateLocaltime)
            {
                if (valueItem->toNumber() > item->toNumber())
                {
                    return false;
                }
            }
            else if (valueItem && valueItem->descriptor().suffix == RConfigLocalTime)
            {
                const QDateTime t1 = QDateTime::fromMSecsSinceEpoch(item->toNumber());
                if (now.time() > t1.time())
                {
                    return false;
                }
            }
        }
        else if (c->op() == RuleCondition::OpGreaterThan)
        {
            if (item->toNumber() <= c->numericValue())
            {
                return false;
            }

            if (item == eItem && e.numPrevious() > c->numericValue())
            {
                return false; // must become >
            }
        }
        else if (c->op() == RuleCondition::OpLowerThan)
        {
            if (item->toNumber() >= c->numericValue())
            {
                return false;
            }

            if (item == eItem && e.numPrevious() < c->numericValue())
            {
                return false; // must become <
            }
        }
        else if (c->op() == RuleCondition::OpDx)
        {
            if (item != eItem)
            {
                return false;
            }

            if (eItem->descriptor().suffix == RStateLastUpdated)
            {}
            else if (eItem->descriptor().suffix == RAttrLastAnnounced)
            {}
            else if (eItem->descriptor().suffix == RConfigLocalTime)
            {}
            else if (e.num() == e.numPrevious())
            {
                return false;
            }
        }
        else if (c->op() == RuleCondition::OpDdx)
        {
            if (eItem->descriptor().suffix != RConfigLocalTime)
            {
                return false;
            }

            if (!item->lastChanged().isValid())
            {
                return false;
            }

            QDateTime dt = item->lastChanged().addSecs(c->seconds());
            if (dt <= previousNow || dt > now)
            {
                return false;
            }
        }
        else if (c->op() == RuleCondition::OpStable)
        {
            if (!item->lastSet().isValid())
            {
                return false;
            }

            QDateTime dt = item->lastChanged().addSecs(c->seconds());
            if (now.secsTo(dt) > 0)
            {
                return false;
            }
        }
        else if (c->op() == RuleCondition::OpIn && c->suffix() == RConfigLocalTime)
        {
            const QTime t = now.time();
            const QTime pt = previousNow.time();

            if (eItem->descriptor().suffix == RConfigLocalTime && (c->time0() <= pt || c->time0() > t))
            {
                return false; // Only trigger on start time
            }

            if (!c->weekDayEnabled(now.date().dayOfWeek()))
            {
                return false;
            }

            if (c->time0() < c->time1() && // 8:00 - 16:00
                (t >= c->time0() && t <= c->time1()))
            {
            }
            else if (c->time0() > c->time1() && // 20:00 - 4:00
                (t >= c->time0() || t <= c->time1()))
                // 20:00 - 0:00  ||  0:00 - 4:00
            {
            }
            else
            {
                return false;
            }
        }
        else if (c->op() == RuleCondition::OpNotIn && c->suffix() == RConfigLocalTime)
        {
            const QTime t = now.time();
            const QTime pt = previousNow.time();

            if (eItem->descriptor().suffix == RConfigLocalTime && (c->time1() <= pt || c->time1() > t))
            {
                return false; // Only trigger on end time
            }

            if (!c->weekDayEnabled(now.date().dayOfWeek()))
            {
                return false;
            }

            if (c->time0() < c->time1() && // 8:00 - 16:00
                (t <= c->time0() || t >= c->time1()))
                // 0:00 - 8:00   || 16.00 - 0.00
            {
            }
            else if (c->time0() > c->time1() && // 20:00 - 4:00
                (t <= c->time0() && t >= c->time1()))
            {
            }
            else
            {
                return false;
            }
        }
        else
        {
            DBG_Printf(DBG_ERROR, "error: rule (%s) operator %s not supported\n", qPrintable(rule.id()), qPrintable(c->ooperator()));
            return false;
        }
    }

    return true;
}

/*! Index rules related resource item triggers.
    \param rule - the rule to index
 */
void DeRestPluginPrivate::indexRuleTriggers(Rule &rule)
{
    ResourceItem *itemDx = nullptr;
    ResourceItem *itemDdx = nullptr;
    std::vector<ResourceItem*> items;

    for (const RuleCondition &c : rule.conditions())
    {
        Resource *resource = getResource(c.resource(), c.id());
        ResourceItem *item = resource ? resource->item(c.suffix()) : nullptr;

        if (!resource || !item)
        {
            continue;
//            DBG_Printf(DBG_INFO, "resource %s : %s id: %s (cond: %s) not found --> disable rule\n",
//                       c->resource(), c->suffix(),
//                       qPrintable(c->id()), qPrintable(c->address()));
        }

        if (!c.id().isEmpty())
        {
            DBG_Printf(DBG_INFO_L2, "\t%s/%s/%s op: %s\n", c.resource(), qPrintable(c.id()), c.suffix(), qPrintable(c.ooperator()));
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "\t%s : %s op: %s\n", c.resource(), c.suffix(), qPrintable(c.ooperator()));
        }

        if (c.op() == RuleCondition::OpDx)
        {
            DBG_Assert(itemDx == nullptr);
            DBG_Assert(itemDdx == nullptr);
            itemDx = item;
        }
        else if (c.op() == RuleCondition::OpDdx)
        {
            DBG_Assert(itemDx == nullptr);
            DBG_Assert(itemDdx == nullptr);
            itemDdx = item;
        }
        else if (c.op() == RuleCondition::OpStable) { }
        else if (c.op() == RuleCondition::OpNotStable) { }
        else
        {
            items.push_back(item);
        }
    }

    if (itemDx)
    {
        items.clear();
        items.push_back(itemDx);
    }
    else if (itemDdx)
    {
        Resource *r = getResource(RConfig);
        itemDdx = r ? r->item(RConfigLocalTime) : nullptr;
        DBG_Assert(r != nullptr);
        DBG_Assert(itemDdx != nullptr);
        items.clear();
        if (itemDdx)
        {
            items.push_back(itemDdx);
        }
    }

    for (ResourceItem *item : items)
    {
        item->inRule(rule.handle());
        DBG_Printf(DBG_INFO_L2, "\t%s (trigger)\n", item->descriptor().suffix);
    }
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
        // check webhook
        if (ai->address().startsWith(QLatin1String("http")))
        {
            if (handleWebHook(*ai) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
            continue;
        }

        if (ai->method() != QLatin1String("PUT") && ai->method() != QLatin1String("POST"))
            return;


        QStringList path = ai->address().split(QChar('/'), QString::SkipEmptyParts);

        if (path.isEmpty()) // at least: /config, /groups, /lights, /sensors
            return;

        QHttpRequestHeader hdr(ai->method(), ai->address());

        // paths start with /api/<apikey/ ...>
        path.prepend(rule.owner()); // apikey
        path.prepend(QLatin1String("api")); // api

        ApiRequest req(hdr, path, nullptr, ai->body());
        ApiResponse rsp;
        rsp.httpStatus = HttpStatusServiceUnavailable;

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
        else if (path[2] == QLatin1String("schedules"))
        {
            if (handleSchedulesApi(req, rsp) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
        }
        else if (path[2] == QLatin1String("scenes"))
        {
            if (handleScenesApi(req, rsp) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
        }
        else if (path[2] == QLatin1String("sensors"))
        {
            if (handleSensorsApi(req, rsp) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
        }
        else if (path[2] == QLatin1String("config"))
        {
            if (handleConfigFullApi(req, rsp) == REQ_NOT_HANDLED)
            {
                return;
            }
            triggered = true;
        }
        else if (path[2] == QLatin1String("rules"))
        {
            if (handleRulesApi(req, rsp) == REQ_NOT_HANDLED)
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

        if (rsp.httpStatus != HttpStatusOk)
        {
            DBG_Printf(DBG_INFO, "trigger rule %s - %s failed with status %s\n", qPrintable(rule.id()), qPrintable(rule.name()), rsp.httpStatus);
            return;
        }
    }

    if (triggered)
    {
        rule.m_lastTriggered = QDateTime::currentDateTimeUtc();
        rule.setTimesTriggered(rule.timesTriggered() + 1);
        updateEtag(rule.etag);
        updateEtag(gwConfigEtag);
        queSaveDb(DB_RULES, DB_HUGE_SAVE_DELAY);
    }
}

/*! Sends a HTTP request aka webhook based on a rule action.
    \param action - the action holding the request details
 */
int DeRestPluginPrivate::handleWebHook(const RuleAction &action)
{
    QNetworkRequest req(QUrl(action.address()));

    QBuffer *data = new QBuffer(this);
    DBG_Assert(data);
    if (!data)
    {
        return REQ_NOT_HANDLED;
    }

    data->setData(action.body().toUtf8());

    QNetworkReply *reply = webhookManager->sendCustomRequest(req, action.method().toLatin1(), data);
    DBG_Assert(reply);
    if (reply)
    {
        reply->setProperty("buf", QVariant::fromValue(data));
        return REQ_READY_SEND;
    }

    return REQ_NOT_HANDLED;
}

/*! Handler for finished webhooks.
  */
void DeRestPluginPrivate::webhookFinishedRequest(QNetworkReply *reply)
{
    if (!reply)
    {
        return;
    }

    if (reply->property("buf").canConvert<QBuffer*>())
    {
        QBuffer *buf  = reply->property("buf").value<QBuffer*>();
        buf->deleteLater();
    }

    DBG_Printf(DBG_INFO, "Webhook finished: %s (code: %d)\n", qPrintable(reply->url().toString()), reply->error());

    if (DBG_IsEnabled(DBG_HTTP))
    {
        for (const auto &hdr : reply->rawHeaderPairs())
        {
            DBG_Printf(DBG_HTTP, "%s: %s\n", qPrintable(hdr.first), qPrintable(hdr.second));
        }

        QByteArray data = reply->readAll();
        if (!data.isEmpty())
        {
            DBG_Printf(DBG_HTTP, "%s\n", qPrintable(data));
        }
    }

    reply->deleteLater();
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

    //triggerRuleIfNeeded(rule);

    if (bindingQueue.size() < 16)
    {
        if (rule.state() == Rule::StateNormal)
        {
            if ((rule.lastBindingVerify + Rule::MaxVerifyDelay) < idleTotalCounter)
            {
                rule.lastBindingVerify = idleTotalCounter;
                queueCheckRuleBindings(rule);
            }
        }
    }

    verifyRuleIter++;
    if (verifyRulesTimer->interval() != NORMAL_RULE_CHECK_INTERVAL_MS)
    {
        verifyRulesTimer->setInterval(NORMAL_RULE_CHECK_INTERVAL_MS);
    }
}

/*! Trigger fast checking of rules related to the resource. */
void DeRestPluginPrivate::indexRulesTriggers()
{
    fastRuleCheck.clear();
    for (const Rule &rule : rules)
    {
        fastRuleCheck.push_back(rule.handle());
    }

    if (!fastRuleCheckTimer->isActive() && !fastRuleCheck.empty())
    {
        fastRuleCheckTimer->start();
    }
}

/*! Checks one rule from the fast check queue per event loop cycle. */
void DeRestPluginPrivate::fastRuleCheckTimerFired()
{
    for (int &handle : fastRuleCheck)
    {
        if (handle == 0)
        {
            continue;  // already checked
        }

        for (Rule &rule: rules)
        {
            if (rule.handle() == handle)
            {
                DBG_Printf(DBG_INFO_L2, "index resource items for rules, handle: %d (%s)\n", rule.handle(), qPrintable(rule.name()));
                indexRuleTriggers(rule);
                fastRuleCheckTimer->start(); // handle in next event loop cycle
                handle = 0; // mark checked
                return;
            }
        }
        handle = 0; // mark checked (2)
    }

    // all done
    fastRuleCheck.clear();
}

/*! Triggers rules based on events. */
void DeRestPluginPrivate::handleRuleEvent(const Event &e)
{
    Resource *resource = getResource(e.resource(), e.id());
    ResourceItem *item = resource ? resource->item(e.what()) : nullptr;
    const ResourceItem *localTime = config.item(RConfigLocalTime);
    const QDateTime now = localTime
      ? QDateTime::fromMSecsSinceEpoch(localTime->toNumber())
      : QDateTime::currentDateTime();
    const QDateTime previousNow = (localTime && localTime->toNumberPrevious() > 0)
      ? QDateTime::fromMSecsSinceEpoch(localTime->toNumberPrevious())
      : now.addSecs(-1);

    if (!resource || !item || item->rulesInvolved().empty())
    {
        return;
    }

    if (!e.id().isEmpty())
    {
        DBG_Printf(DBG_INFO, "rule event %s/%s/%s: %d -> %d\n", e.resource(), qPrintable(e.id()), e.what(), e.numPrevious(), e.num());
    }
    else
    {
        DBG_Printf(DBG_INFO_L2, "rule event /%s: %s -> %s (%lldms)\n", e.what(), qPrintable(previousNow.toString("hh:mm:ss.zzz")), qPrintable(now.toString("hh:mm:ss.zzz")), previousNow.msecsTo(now));
    }


    // QElapsedTimer t;
    // t.start();
    std::vector<size_t> rulesToTrigger;
    for (int handle : item->rulesInvolved())
    {
        for (size_t i = 0; i < rules.size(); i++)
        {
            if (rules[i].handle() != handle)
            {
                continue;
            }

            if (evaluateRule(rules[i], e, resource, item, now, previousNow))
            {
                rulesToTrigger.push_back(i);
            }
        }
    }

    for (size_t i : rulesToTrigger)
    {
        DBG_Assert(i < rules.size());
        if (i < rules.size())
        {
            triggerRule(rules[i]);
        }
    }

    // int dt = t.elapsed();
    // if (dt > 0)
    // {
    //     DBG_Printf(DBG_INFO_L2, "trigger rule events took %d ms\n", dt);
    // }
}
