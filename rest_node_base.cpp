/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QTime>
#include "de_web_plugin_private.h"

/*! Constructor.
 */
RestNodeBase::RestNodeBase() :
    m_node(0),
    m_available(false),
    m_mgmtBindSupported(true),
    m_needSaveDatabase(false),
    m_read(0),
    m_lastRead(0),
    m_lastAttributeReportBind(0)
{
    QTime t = QTime::currentTime();

    for (int i = 0; i < 16; i++)
    {
        m_lastRead.push_back(0);
        m_nextReadTime.push_back(t);
    }
}

/*! Deconstructor.
 */
RestNodeBase::~RestNodeBase()
{
}

/*! Returns the core node object.
 */
deCONZ::Node *RestNodeBase::node()
{
    return m_node;
}

/*! Sets the core node object.
    \param node the core node
 */
void RestNodeBase::setNode(deCONZ::Node *node)
{
    m_node = node;
}

/*! Returns the modifiable address.
 */
deCONZ::Address &RestNodeBase::address()
{
    return m_addr;
}

/*! Returns the const address.
 */
const deCONZ::Address &RestNodeBase::address() const
{
    return m_addr;
}

/*! Returns true if the node is available.
 */
bool RestNodeBase::isAvailable() const
{
    return false;
}

/*! Returns if the data needs to be saved to database.
 */
bool RestNodeBase::needSaveDatabase() const
{
    return m_needSaveDatabase;
}

/*! Sets if the data needs to be saved to database.
    \param needSave true if needs to be saved
 */
void RestNodeBase::setNeedSaveDatabase(bool needSave)
{
    m_needSaveDatabase = needSave;
}

/*! Returns the unique identifier of the node.
 */
const QString &RestNodeBase::id() const
{
    return m_id;
}

/*! Sets the identifier of the node.
    \param id the unique identifier
 */
void RestNodeBase::setId(const QString &id)
{
    m_id = id;
}

/*! Returns the nodes unique Id.
    The MAC address of the device with a unique endpoint id in the form: AA:BB:CC:DD:EE:FF:00:11-XX
 */
const QString &RestNodeBase::uniqueId() const
{
    const Resource *r = dynamic_cast<const Resource*>(this);
    const ResourceItem *item = r ? r->item(RAttrUniqueId) : 0;
    if (item)
    {
        return item->toString();
    }
    return m_uid;
}

/*! Sets the sensor unique Id.
    The MAC address of the device with a unique endpoint id in the form: AA:BB:CC:DD:EE:FF:00:11-XX
    \param uid the sensor unique Id
 */
void RestNodeBase::setUniqueId(const QString &uid)
{
    Resource *r = dynamic_cast<Resource*>(this);
    ResourceItem *item = r ? r->addItem(DataTypeString, RAttrUniqueId) : 0;
    if (item)
    {
        item->setValue(uid);
    }
    m_uid = uid;
}

/*! Check if some data must be queried from the node.
    \param readFlags or combined bitmap of READ_* values
    \return true if every flag in readFlags is set
*/
bool RestNodeBase::mustRead(uint32_t readFlags)
{
    if ((m_read & readFlags) == readFlags)
    {
        return true;
    }

    return false;
}

/*! Enables all flags given in \p readFlags in the read set.
    \param readFlags or combined bitmap of READ_* values
 */
void RestNodeBase::enableRead(uint32_t readFlags)
{
    m_read |= readFlags;
}

/*! Clears all flags given in \p readFlags in the read set.
    \param readFlags or combined bitmap of READ_* values
 */
void RestNodeBase::clearRead(uint32_t readFlags)
{
    m_read &= ~readFlags;
}

/*! Returns the time than the next auto reading is queued.
    \param item the item to read
 */
const QTime &RestNodeBase::nextReadTime(uint32_t item) const
{
    for (size_t i = 0; i < m_nextReadTime.size(); i++)
    {
        if ((1u << i) == item)
        {
            return m_nextReadTime[i];
        }
    }
    Q_ASSERT(0 || "m_nextReadTime[] too small");
    return m_invalidTime;
}

/*! Sets the time than the next auto reading should be queued.
    \param item the item to read
    \param time the time for reading
 */
void RestNodeBase::setNextReadTime(uint32_t item, const QTime &time)
{
    for (size_t i = 0; i < m_nextReadTime.size(); i++)
    {
        if ((1u << i) == item)
        {
            m_nextReadTime[i] = time;
            return;
        }
    }
    Q_ASSERT(0 || "m_nextReadTime[] too small");
}

/*! Returns the value of the idleTotalCounter than the last reading happend.
    \param item the item to read
 */
int RestNodeBase::lastRead(uint32_t item) const
{
    for (size_t i = 0; i < m_lastRead.size(); i++)
    {
        if ((1u << i) == item)
        {
            return m_lastRead[i];
        }
    }
    Q_ASSERT(0 || "m_lastRead[] too small");
    return 0;
}

/*! Sets the last read counter.
    \param item the item to read
    \param lastRead copy of idleTotalCounter
 */
void RestNodeBase::setLastRead(uint32_t item, int lastRead)
{
    for (size_t i = 0; i < m_lastRead.size(); i++)
    {
        if ((1u << i) == item)
        {
            m_lastRead[i] = lastRead;
            return;
        }
    }
    Q_ASSERT(0 || "m_lastRead[] too small");
}

/*! Returns the value of the idleTotalCounter than the last attribute report binding was done.
 */
int RestNodeBase::lastAttributeReportBind() const
{
    return m_lastAttributeReportBind;
}

/*! Sets idleTotalCounter of last attribute report binding.
    \param lastBind copy of idleTotalCounter
 */
void RestNodeBase::setLastAttributeReportBind(int lastBind)
{
    m_lastAttributeReportBind = lastBind;
}

/*! Returns true if mgmt bind request/response are supported.
 */
bool RestNodeBase::mgmtBindSupported() const
{
    return m_mgmtBindSupported;
}

/*! Sets the query binding table supported flag;
    \param supported - query binding table supported flag
 */
void RestNodeBase::setMgmtBindSupported(bool supported)
{
    m_mgmtBindSupported = supported;
}

/*! Sets a numeric ZCL attribute value.

    A timestamp will begenerated automatically.
    \param updateType - specifies if value came by ZCL attribute read or report command
    \param clusterId - the cluster id of the value
    \param attributeId - the attribute id of the value
    \param value - the value data
 */
void RestNodeBase::setZclValue(NodeValue::UpdateType updateType, quint16 clusterId, quint16 attributeId, const deCONZ::NumericUnion &value)
{
    QDateTime now = QDateTime::currentDateTime();
    std::vector<NodeValue>::iterator i = m_values.begin();
    std::vector<NodeValue>::iterator end = m_values.end();

    for (; i != end; ++i)
    {
        if (i->clusterId == clusterId &&
            i->attributeId == attributeId)
        {
            i->updateType = updateType;
            i->value = value;
            int dt = i->timestamp.secsTo(now);
            i->timestamp = now;

            if (updateType == NodeValue::UpdateByZclReport)
            {
                i->timestampLastReport = now;
            }
            DBG_Printf(DBG_INFO_L2, "update ZCL value 0x%04X/0x%04X for 0x%016llX after %d s\n", clusterId, attributeId, address().ext(), dt);
            return;
        }
    }

    NodeValue val;
    val.timestamp = now;
    if (updateType == NodeValue::UpdateByZclReport)
    {
        val.timestampLastReport = now;
    }
    val.clusterId = clusterId;
    val.attributeId = attributeId;
    val.updateType = updateType;
    val.value = value;

    DBG_Printf(DBG_INFO_L2, "added ZCL value 0x%04X/0x%04X for 0x%016llX\n", clusterId, attributeId, address().ext());

    m_values.push_back(val);
}

/*! Returns a numeric ZCL attribute value.

    If the value couldn't be found the NodeValue::timestamp field holds a invalid QTime.
    \param clusterId - the cluster id of the value
    \param attributeId - the attribute id of the value
 */
const NodeValue &RestNodeBase::getZclValue(quint16 clusterId, quint16 attributeId) const
{
    std::vector<NodeValue>::const_iterator i = m_values.begin();
    std::vector<NodeValue>::const_iterator end = m_values.end();

    for (; i != end; ++i)
    {
        if (i->clusterId == clusterId &&
            i->attributeId == attributeId)
        {
            return *i;
        }
    }

    return m_invalidValue;
}

/*! Returns a numeric ZCL attribute value.

    If the value couldn't be found the NodeValue::timestamp field holds a invalid QTime.
    \param clusterId - the cluster id of the value
    \param attributeId - the attribute id of the value
 */
NodeValue &RestNodeBase::getZclValue(quint16 clusterId, quint16 attributeId)
{
    std::vector<NodeValue>::iterator i = m_values.begin();
    std::vector<NodeValue>::iterator end = m_values.end();

    for (; i != end; ++i)
    {
        if (i->clusterId == clusterId &&
            i->attributeId == attributeId)
        {
            return *i;
        }
    }

    return m_invalidValue;
}

/*! Returns ZCL attribute values.
 */
std::vector<NodeValue> &RestNodeBase::zclValues()
{
    return m_values;
}

/*! Returns ZCL attribute values.
 */
const std::vector<NodeValue> &RestNodeBase::zclValues() const
{
    return m_values;
}

/*! Returns timestamp of last rx. */
const QDateTime &RestNodeBase::lastRx() const
{
    return m_lastRx;
}

/*! Mark received command. */
void RestNodeBase::rx()
{
    m_lastRx = QDateTime::currentDateTime();
}
