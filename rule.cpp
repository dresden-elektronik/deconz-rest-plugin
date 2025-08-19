/*
 * Copyright (c) 2017-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QRegExp>
#include "ias_ace.h"
#include "rule.h"

static int _ruleHandle = 1;

/*! Constructor. */
Rule::Rule() :
    m_state(StateNormal),
    //m_id(QLatin1String("none")),
    m_handle(-1),
    //m_name("notSet"),
    //m_creationtime("notSet"),
    m_timesTriggered(0),
    m_triggerPeriodic(0),
    //m_owner("notSet"),
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
    m_handle = _ruleHandle++;
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

/*! Returns the timestamp the rule was last triggered.
 */
const QDateTime &Rule::lastTriggered() const
{
    return m_lastTriggered;
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
quint32 Rule::timesTriggered() const
{
    return m_timesTriggered;
}

/*! Sets the count the rule was triggered.
    \param timesTriggered the count the rule was triggered
 */
void Rule::setTimesTriggered(quint32 timesTriggered)
{
    m_timesTriggered = timesTriggered;
}

/*! Returns the trigger periodic time value in milliseconds.
      val  < 0 trigger disabled
      val == 0 trigger on event
      val  > 0 trigger every <val> ms
 */
int Rule::triggerPeriodic() const
{
    return m_triggerPeriodic;
}

void Rule::setTriggerPeriodic(int ms)
{
    m_triggerPeriodic = ms;
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

/*! Returns true if rule is enabled.
 */
bool Rule::isEnabled() const
{
    return m_status == QLatin1String("enabled");
}

/*! Returns the unique rule handle (only valid for this session).
 */
int Rule::handle() const
{
    return m_handle;
}

/*! Transfers actions into JSONString.
    \param actions vector<Action>
 */
QString Rule::actionsToString(const std::vector<RuleAction> &actions)
{
    QString jsonString = QLatin1String("[");
    auto i = actions.cbegin();
    const auto i_end = actions.cend();

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
    QVariantList ls;

    auto i = conditions.cbegin();
    const auto iend = conditions.cend();

    for (; i != iend; ++i)
    {
        QVariantMap map;
        map["address"] = i->address();
        map["operator"] = i->ooperator();
        if (i->value().isValid())
        {
            map["value"] = i->value();
        }
        ls.append(map);
    }

    return Json::serialize(ls);
}

/*! Parse a JSON string into RuleAction array.
    \param json - a JSON list of actions
 */
std::vector<RuleAction> Rule::jsonToActions(const QString &json)
{
    bool ok;
    std::vector<RuleAction> actions;
    QVariantList var = Json::parse(json, ok).toList();

    if (!ok)
    {
        return actions;
    }

    auto i = var.cbegin();
    const auto end = var.cend();

    for (; i != end; ++i)
    {
        RuleAction action;
        QVariantMap map = i->toMap();
        action.setAddress(map["address"].toString());

        QVariantMap bodymap = i->toMap()["body"].toMap();
        action.setBody(Json::serialize(bodymap));
        action.setMethod(map["method"].toString());
        actions.push_back(action);
    }

    return actions;
}

std::vector<RuleCondition> Rule::jsonToConditions(const QString &json)
{
    bool ok;
    QVariantList var = Json::parse(json, ok).toList();
    std::vector<RuleCondition> conditions;

    if (!ok)
    {
        DBG_Printf(DBG_INFO, "failed to parse rule conditions: %s\n", qPrintable(json));
        return conditions;
    }

    auto i = var.cbegin();
    const auto end = var.cend();

    for (; i != end; ++i)
    {
        const RuleCondition cond(i->toMap());
        if (cond.isValid())
        {
            conditions.push_back(cond);
        }
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
    Either "POST", "PUT", "DELETE" for local addresses.
    \param method the action method
 */
void RuleAction::setMethod(const QString &method)
{
    DBG_Assert(method == "POST" || method == "PUT" || method == "DELETE" || method == "GET");
    if (!(method == "POST" || method == "PUT" || method == "DELETE" || method == "GET"))
    {
        DBG_Printf(DBG_INFO, "actions method must be either GET, POST, PUT or DELETE\n");
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
    m_prefix(nullptr),
    m_suffix(nullptr),
    m_op(OpUnknown),
    m_num(0),
    m_weekDays(127) // default all days enabled
{
}

/*! Constructs a RuleCondition from the data given in \p map.

    The RuleCondition::isValid() method should be used to verify
    the object was constructed sucessfully.
 */
RuleCondition::RuleCondition(const QVariantMap &map)
{
    bool ok = false;
    m_address = map["address"].toString();
    m_operator = map["operator"].toString();
    m_value = map["value"];

    // cache id
    if (m_address.startsWith(QLatin1String(RSensors)) ||
        m_address.startsWith(QLatin1String(RGroups)) ||
        m_address.startsWith(QLatin1String(RLights)) ||
        m_address.startsWith(QLatin1String(RAlarmSystems))) // /sensors/<id>/state/buttonevent, ...
    {
        QStringList addrList = m_address.split('/', SKIP_EMPTY_PARTS);
        if (addrList.size() > 1)
        {
            m_id = addrList[1];
        }
    }

    if (m_address.startsWith(QLatin1String(RSensors)))
    {
        m_prefix = RSensors;
        if (m_address.endsWith(QLatin1String("/illuminance")))
        { // convert old to new style
            m_address.replace(QLatin1String("/illuminance"), QLatin1String("/lux"));
        }
    }
    else if (m_address.startsWith(QLatin1String(RConfig)))
    {
        m_prefix = RConfig;
    }
    else if (m_address.startsWith(QLatin1String(RGroups)))
    {
        m_prefix = RGroups;
    }
    else if (m_address.startsWith(QLatin1String(RLights)))
    {
        m_prefix = RLights;
    }
    else if (m_address.startsWith(QLatin1String(RAlarmSystems)))
    {
        m_prefix = RAlarmSystems;


    }

    ResourceItemDescriptor rid;

    m_suffix = getResourceItemDescriptor(m_address, rid) ? rid.suffix
                                                         : RInvalidSuffix;

    if (m_operator == QLatin1String("eq")) { m_op = OpEqual; }
    else if (m_operator == QLatin1String("ne")) { m_op = OpNotEqual; }
    else if (m_operator == QLatin1String("gt")) { m_op = OpGreaterThan; }
    else if (m_operator == QLatin1String("lt")) { m_op = OpLowerThan; }
    else if (m_operator == QLatin1String("dx")) { m_op = OpDx; }
    else if (m_operator == QLatin1String("ddx")) { m_op = OpDdx; }
    else if (m_operator == QLatin1String("in")) { m_op = OpIn; }
    else if (m_operator == QLatin1String("not in")) { m_op = OpNotIn; }
    else if (m_operator == QLatin1String("stable")) { m_op = OpStable; }
    else { m_op = OpUnknown; }

    // extract proper datatype
    if (m_value.type() == QVariant::String)
    {
        const QString str = m_value.toString();

        if (m_op == OpDdx || m_op == OpStable)
        {
            QTime t = QTime::fromString(str, "'PT'hh:mm:ss");
            if (!t.isValid())
            {
                m_op = OpUnknown; // invalid
                return;
            }
            m_time0 = t;
            // cache duration in seconds
            m_num = QTime(0,0,0).secsTo(t);
        }
        else if (m_op == OpIn || m_op == OpNotIn)
        {
            QStringList interval = str.split('/', SKIP_EMPTY_PARTS);
            if (interval.size() == 3)
            {
                const QRegExp rx("W([0-9]{1,3})");
                const QString weekDays = interval.takeFirst();
                if (rx.exactMatch(weekDays))
                {
                    const uint w = rx.cap(1).toUInt(&ok);
                    if (!ok || w > 127)
                    {
                        return; // invalid
                    }

                    m_weekDays = static_cast<quint8>(w);
                }
            }

            if (interval.size() != 2)
            {
                m_op = OpUnknown; // invalid
                return;
            }

            QTime t0 = QTime::fromString(interval[0], "'T'hh:mm:ss");
            QTime t1 = QTime::fromString(interval[1], "'T'hh:mm:ss");
            if (t0.isValid() && t1.isValid())
            {
                m_time0 = t0;
                m_time1 = t1;
            } else { m_op = OpUnknown; } // mark invalid
        }
        else if (str == QLatin1String("true") ||
                 str == QLatin1String("false"))
        {
            m_value = m_value.toBool();
        }
        else if ((m_op == OpGreaterThan || m_op == OpLowerThan) && m_suffix == RStateLocaltime && str.endsWith(QLatin1String("/localtime")))
        {
            // TODO dynamically referring to other resources in conditions might be useful in general

            // /config/localtime
            if (str.endsWith(QLatin1String(RConfigLocalTime)))
            {
                m_valuePrefix = RConfig;
                m_valueSuffix = RConfigLocalTime;
            }
            // /sensors/51/state/localtime
            else if (str.startsWith(QLatin1String(RSensors)) && str.endsWith(QLatin1String(RStateLocaltime)))
            {
                const QStringList ls = str.split('/', SKIP_EMPTY_PARTS); // cache resource id
                // [ "sensors", "51", "state", "localtime" ]
                if (ls.size() == 4)
                {
                    m_valuePrefix = RSensors;
                    m_valueSuffix = RStateLocaltime;
                    m_valueId = ls[1];
                }
                else
                {
                    m_op = OpUnknown; // invalid
                }
            }
            else
            {
                m_op = OpUnknown; // invalid
            }
        }
        else if (m_op == OpEqual || m_op == OpNotEqual || m_op == OpGreaterThan || m_op == OpLowerThan)
        {
            if (rid.suffix == RStateArmState)
            {
                // transform from string to number
                int num = IAS_PanelStatusFromString(str);
                if (num >= 0)
                {
                    m_num = num;
                } else { m_op = OpUnknown; } // mark invalid
            }
            else
            {
                int num = str.toInt(&ok);
                if (ok)
                {
                    m_value = static_cast<double>(num);
                } else { m_op = OpUnknown; } // mark invalid
            }
        }
    }

    if (m_value.type() == QVariant::Double ||
        m_value.type() == QVariant::UInt ||
        m_value.type() == QVariant::Int)
    {
        m_num = m_value.toInt(&ok);
        if (!ok) { m_num = 0; m_op = OpUnknown; }
    }
    else if (m_value.type() == QVariant::Bool)
    {
        m_num = m_value.toBool() ? 1 : 0;
    }
    else if (m_value.type() == QVariant::Time)
    {
    }
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
    DBG_Assert((aOperator == "eq") || (aOperator == "ne") || (aOperator == "gt") || (aOperator == "lt") || (aOperator == "dx"));
    if (!((aOperator == "eq") || (aOperator == "ne") || (aOperator == "gt") || (aOperator == "lt") || (aOperator == "dx")))
    {
        DBG_Printf(DBG_INFO, "actions operator must be either 'eq', 'ne', 'gt', 'lt' or 'dx'\n");
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

/*! Returns the condition value.
 */
const QVariant &RuleCondition::value() const
{
    return m_value;
}

/*! Sets the condition value.
    The resource attribute is compared to this value using the given operator.
    The value is cast to the data type of the resource attribute (in case of time, casted to a timePattern).
    If the cast fails or the operator does not support the data type the value is cast to the rule is rejected.
    \param value the condition value
 */
void RuleCondition::setValue(const QVariant &value)
{
    m_value = value;
}

bool RuleCondition::operator==(const RuleCondition &other) const
{
    return (m_address == other.m_address &&
            m_operator == other.m_operator &&
            m_value == other.m_value);
}

/*! Returns operator as enum.
 */
RuleCondition::Operator RuleCondition::op() const
{
    return m_op;
}

/*! Returns resource id of address.
 */
const QString &RuleCondition::id() const
{
    return m_id;
}

/*! Returns resource id of address given in a value.
 */
const QString &RuleCondition::valueId() const
{
    return m_valueId;
}

/*! Returns value as int (for numeric and bool types).
 */
int RuleCondition::numericValue() const
{
    return m_num;
}

/*! Returns value as duration in seconds (for operators OpDdx, OpStable, OpIn and OpNotIn).
 */
int RuleCondition::seconds() const
{
    return m_num;
}

/*! Returns start time (for operators OpIn and OpNotIn).
 */
const QTime &RuleCondition::time0() const
{
    return m_time0;
}

/*! Returns end time (for operators OpIn and OpNotIn).
 */
const QTime &RuleCondition::time1() const
{
    return m_time1;
}

/*! Returns true if the given weekday is enabled (for operators OpIn and OpNotIn).

    The condition needs format of W[bbb]/T[hh]:[mm]:[ss]/T[hh]:[mm]:[ss].
    If W[bbb] is not specified all days are enabled as of W127.
    \param day - 1 Monday .. 7 Sunday
 */
bool RuleCondition::weekDayEnabled(const int day) const
{
    // bbb = 0MTWTFSS – e.g. Tuesdays is 00100000 = 32
    DBG_Assert(day >= 0 && day <= 7);
    return (m_weekDays & (1 << (7 - day))) != 0;
}

/*! Returns the related Resource prefix like RSensors, RLights, etc.
 */
const char *RuleCondition::resource() const
{
    return m_prefix;
}

/*! Returns the Resource suffix like RStateButtonevent.
 */
const char *RuleCondition::suffix() const
{
    return m_suffix;
}

/*! Returns the related Resource prefix like RSensors, RLights, etc. of the value,
    if value is pointing to another resource. Otherwise \p nullptr is returned.
 */
const char *RuleCondition::valueResource() const
{
    return m_valuePrefix;
}

/*! Returns the Resource suffix like RStateButtonevent of the value,
    if value is pointing to another resource. Otherwise \p nullptr is returned.
 */
const char *RuleCondition::valueSuffix() const
{
    return m_valueSuffix;
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
