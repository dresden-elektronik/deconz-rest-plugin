/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "rule.h"

/*! Constructor. */
Rule::Rule() :
    lastVerify(0),
    m_state(StateNormal),
    m_id("notSet"),
    m_name("notSet"),
    m_lastTriggered("none"),
    m_creationtime("notSet"),
    m_timesTriggered(0),
    m_owner("notSet"),
    m_status("enabled")
{
}

/*! Returns the rule state.
 */
Rule::State Rule::state() const
{
    return m_state;
}

/*! Sets the rule state.
    \param state the rule state
 */
void Rule::setState(State state)
{
    m_state = state;
}

/*! Returns the rule id.
 */
const QString &Rule::id() const
{
    return m_id;
}

/*! Sets the rule id.
    \param id the rule id
 */
void Rule::setId(const QString &id)
{
    m_id = id;
}

/*! Returns the rule name.
 */
const QString &Rule::name() const
{
    return m_name;
}

/*! Sets the rule name.
    \param name the rule name
 */
void Rule::setName(const QString &name)
{
    m_name = name;
}

/*! Returns the date the rule was last triggered.
 */
const QString &Rule::lastTriggered() const
{
    return this->m_lastTriggered;
}

/*! Sets the date the rule was last triggered.
    \param lastTriggered the date the rule was last triggered
 */
void Rule::setLastTriggered(const QString &lastTriggered)
{
    this->m_lastTriggered = lastTriggered;
}

/*! Returns the date the rule was created.
 */
const QString &Rule::creationtime() const
{
    return this->m_creationtime;
}

/*! Sets the date the rule was created.
    \param creationtime the date the rule was created
 */
void Rule::setCreationtime(const QString &creationtime)
{
    this->m_creationtime = creationtime;
}

/*! Returns the count the rule was triggered.
 */
const quint32 &Rule::timesTriggered() const
{
    return this->m_timesTriggered;
}

/*! Sets the count the rule was triggered.
    \param timesTriggered the count the rule was triggered
 */
void Rule::setTimesTriggered(const quint32 &timesTriggered)
{
    this->m_timesTriggered = timesTriggered;
}

/*! Returns the owner of the rule.
 */
const QString &Rule::owner() const
{
    return this->m_owner;
}

/*! Sets the owner of the rule.
    \param owner the owner of the rule
 */
void Rule::setOwner(const QString &owner)
{
    this->m_owner = owner;
}

/*! Returns the status of the rule.
 */
const QString &Rule::status() const
{
    return this->m_status;
}

/*! Sets the status of the rule.
    \param status the status of the rule
 */
void Rule::setStatus(const QString &status)
{
    this->m_status = status;
}

/*! Returns the rule conditions.
 */
const std::vector<RuleCondition> &Rule::conditions() const
{
    return this->m_conditions;
}

/*! Sets the rule conditions.
    \param conditions the rule conditions
 */
void Rule::setConditions(const std::vector<RuleCondition> &conditions)
{
    this->m_conditions = conditions;
}

/*! Returns the rule actions.
 */
const std::vector<RuleAction> &Rule::actions() const
{
    return this->m_actions;
}

/*! Sets the rule actions.
    \param actions the rule actions
 */
void Rule::setActions(const std::vector<RuleAction> &actions)
{
    this->m_actions = actions;
}

/*! Transfers actions into JSONString.
    \param actions vector<Action>
 */
QString Rule::actionsToString(const std::vector<RuleAction> &actions)
{
    QString jsonString = QLatin1String("[");
    std::vector<RuleAction>::const_iterator i = actions.begin();
    std::vector<RuleAction>::const_iterator i_end = actions.end();

    for (; i != i_end; ++i)
    {
        jsonString.append(QLatin1String("{\"address\":"));
        jsonString.append(QLatin1String("\"") + i->address() + QLatin1String("\","));
        jsonString.append(QLatin1String("\"body\":") + i->body() + QLatin1String(","));
        jsonString.append(QLatin1String("\"method\":\"") + i->method() + QLatin1String("\"},"));
    }
    jsonString.chop(1);
    jsonString.append(QLatin1String("]"));

    return jsonString;
}

/*! Transfers conditions into JSONString.
    \param conditions vector<Condition>
 */
QString Rule::conditionsToString(const std::vector<RuleCondition> &conditions)
{
    QString jsonString = QLatin1String("[");
    std::vector<RuleCondition>::const_iterator i = conditions.begin();
    std::vector<RuleCondition>::const_iterator i_end = conditions.end();

    for (; i != i_end; ++i)
    {
        jsonString.append(QLatin1String("{\"address\":"));
        jsonString.append(QLatin1String("\"") + i->address() + QLatin1String("\","));
        jsonString.append(QLatin1String("\"operator\":\"") + i->ooperator() + QLatin1String("\","));
        jsonString.append(QLatin1String("\"value\":\"") + i->value() + QLatin1String("\"},"));
    }
    jsonString.chop(1);
    jsonString.append(QLatin1String("]"));

    return jsonString;
}

std::vector<RuleAction> Rule::jsonToActions(const QString &json)
{
    bool ok;
    QVariantList var = (Json::parse(json, ok)).toList();
    QVariantMap map;
    RuleAction action;
    std::vector<RuleAction> actions;

    QVariantList::const_iterator i = var.begin();
    QVariantList::const_iterator i_end = var.end();

    for (; i != i_end; ++i)
    {
        map = i->toMap();
        action.setAddress(map["address"].toString());

        QVariantMap bodymap = (i->toMap()["body"]).toMap();
        QVariantMap::const_iterator b = bodymap.begin();
        QVariantMap::const_iterator bend = bodymap.end();
        QRegExp numbers("^[0-9]\\d*$");

        QString bodystring = "{";
        QString value = b->toString();
        if ((value != "true") && (value != "false") && (!value.contains(numbers)))
        {
            value.prepend("\"");
            value.append("\"");
        }

        for (; b != bend; ++b)
        {
            bodystring.append("\"" + b.key() + "\" : " + value + ", ");
        }
        if (bodystring != "{")
        {
            bodystring.chop(2);
        }
        bodystring.append("}");

        action.setBody(bodystring);
        action.setMethod(map["method"].toString());
        actions.push_back(action);
    }

    return actions;
}

std::vector<RuleCondition> Rule::jsonToConditions(const QString &json)
{
    bool ok;
    QVariantList var = (Json::parse(json, ok)).toList();
    QVariantMap map;
    RuleCondition condition;
    std::vector<RuleCondition> conditions;

    QVariantList::const_iterator i = var.begin();
    QVariantList::const_iterator i_end = var.end();

    for (; i != i_end; ++i)
    {
        map = i->toMap();
        condition.setAddress(map["address"].toString());
        condition.setOperator(map["operator"].toString());
        condition.setValue(map["value"].toString());
        conditions.push_back(condition);
    }

    return conditions;
}


// Action

RuleAction::RuleAction() :
    m_address(""),
    m_method(""),
    m_body("")
{
}

/*! Sets the action address.
    Path to a light resource, a group resource or any other bridge resource
    \param address the action address
 */
void RuleAction::setAddress(const QString &address)
{
    m_address = address;
}

/*! Returns the action address.
 */
const QString &RuleAction::address() const
{
    return m_address;
}

/*! Sets the action method.
    The HTTP method used to send the body to the given address.
    Either "POST", "PUT", "BIND", "DELETE" for local addresses.
    \param method the action method
 */
void RuleAction::setMethod(const QString &method)
{
    DBG_Assert((method == "POST") || (method == "PUT") || (method == "DELETE") || (method == "BIND"));
    if (!((method == "POST") || (method == "PUT") || (method == "DELETE") || (method == "BIND")))
    {
        DBG_Printf(DBG_INFO, "actions method must be either POST, PUT, BIND or DELETE\n");
        return;
    }
    m_method = method;
}

/*! Returns the action method.
 */
const QString &RuleAction::method() const
{
    return m_method;
}

/*! Sets the action body.
    JSON string to be send to the relevant resource.
    \param body the action body
 */
void RuleAction::setBody(const QString &body)
{
    QString str = body;
    m_body = str.replace( " ", "" );
}

bool RuleAction::operator==(const RuleAction &other) const
{
    return(m_address == other.m_address &&
           m_body == other.m_body &&
           m_method == other.m_method);
}

/*! Returns the action body.
 */
const QString &RuleAction::body() const
{
    return m_body;
}


// Condition

RuleCondition::RuleCondition() :
    m_address(""),
    m_operator(""),
    m_value("")
{
}

/*! Sets the condition address.
    Path to an attribute of a sensor resource.
    \param address the condition address
 */
void RuleCondition::setAddress(const QString &address)
{
    m_address = address;
}

/*! Returns the condition address.
 */
const QString &RuleCondition::address() const
{
    return m_address;
}

/*! Sets the condition operator.
    The operator can be 'eq', 'gt', 'lt' or 'dx'
    \param operator the condition operator
 */
void RuleCondition::setOperator(const QString &aOperator)
{
    DBG_Assert((aOperator == "eq") || (aOperator == "gt") || (aOperator == "lt") || (aOperator == "dx"));
    if (!((aOperator == "eq") || (aOperator == "gt") || (aOperator == "lt") || (aOperator == "dx")))
    {
        DBG_Printf(DBG_INFO, "actions operator must be either 'eq', 'gt', 'lt' or 'dx'\n");
        return;
    }

    m_operator = aOperator;
}

/*! Returns the condition address.
 */
const QString &RuleCondition::ooperator() const
{
    return m_operator;
}

/*! Sets the condition value.
    The resource attribute is compared to this value using the given operator.
    The value is cast to the data type of the resource attribute (in case of time, casted to a timePattern).
    If the cast fails or the operator does not support the data type the value is cast to the rule is rejected.
    \param value the condition value
 */
void RuleCondition::setValue(const QString &value)
{
    m_value = value;
}

bool RuleCondition::operator==(const RuleCondition &other) const
{
    return (m_address == other.m_address &&
            m_operator == other.m_operator &&
            m_value == other.m_value);
}

/*! Returns the condition value.
 */
const QString &RuleCondition::value() const
{
    return m_value;
}

/*! Returns true if two BindingTasks are equal.
 */
bool BindingTask::operator==(const BindingTask &rhs) const
{
    if (rhs.action == action &&
        rhs.binding == binding)
    {
        return true;
    }

    return false;
}

/*! Returns true if two BindingTasks are unequal.
 */
bool BindingTask::operator!=(const BindingTask &rhs) const
{
    return !(*this == rhs);
}
