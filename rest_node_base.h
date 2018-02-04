/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_NODE_BASE_H
#define REST_NODE_BASE_H

#include <QDateTime>
#include <deconz.h>

/*! \class NodeValue

    Holds bookkeeping data for numeric ZCL values.
 */
class NodeValue
{
public:
    enum UpdateType { UpdateInvalid, UpdateByZclReport, UpdateByZclRead };

    NodeValue() :
        updateType(UpdateInvalid),
        clusterId(0),
        attributeId(0),
        minInterval(0),
        maxInterval(0)
    {
        value.u64 = 0;
    }
    bool isValid() const { return updateType != UpdateInvalid; }

    QDateTime timestamp;
    QDateTime timestampLastReport;
    QDateTime timestampLastReadRequest;
    QDateTime timestampLastConfigured;
    UpdateType updateType;
    quint16 clusterId;
    quint16 attributeId;
    quint16 minInterval;
    quint16 maxInterval;
    quint8 zclSeqNum; // sequence number for configure reporting
    deCONZ::NumericUnion value;
};


/*! \class RestNodeBase

    The base class for all device representations.
 */
class RestNodeBase
{
public:
    RestNodeBase();
    virtual ~RestNodeBase();
    deCONZ::Node *node();
    void setNode(deCONZ::Node *node);
    deCONZ::Address &address();
    const deCONZ::Address &address() const;
    virtual bool isAvailable() const;
    bool needSaveDatabase() const;
    void setNeedSaveDatabase(bool needSave);
    const QString &id() const;
    void setId(const QString &id);
    const QString &uniqueId() const;
    void setUniqueId(const QString &uid);
    bool mustRead(uint32_t readFlags);
    void enableRead(uint32_t readFlags);
    void clearRead(uint32_t readFlags);
    const QTime &nextReadTime(uint32_t item) const;
    void setNextReadTime(uint32_t item, const QTime &time);
    int lastRead(uint32_t item) const;
    void setLastRead(uint32_t item, int lastRead);
    int lastAttributeReportBind() const;
    void setLastAttributeReportBind(int lastBind);
    bool mgmtBindSupported() const;
    void setMgmtBindSupported(bool supported);
    void setZclValue(NodeValue::UpdateType updateType, quint16 clusterId, quint16 attributeId, const deCONZ::NumericUnion &value);
    const NodeValue &getZclValue(quint16 clusterId, quint16 attributeId) const;
    NodeValue &getZclValue(quint16 clusterId, quint16 attributeId);
    std::vector<NodeValue> &zclValues();
    const std::vector<NodeValue> &zclValues() const;
    const QDateTime &lastRx() const;
    void rx();

private:
    deCONZ::Node *m_node;
    deCONZ::Address m_addr;
    QString m_id;
    QString m_uid;
    bool m_available;
    bool m_mgmtBindSupported;
    bool m_needSaveDatabase;

    uint32_t m_read; // bitmap of READ_* flags
    std::vector<int> m_lastRead; // copy of idleTotalCounter
    int m_lastAttributeReportBind; // copy of idleTotalCounter
    std::vector<QTime> m_nextReadTime;
    QDateTime m_lastRx;

    NodeValue m_invalidValue;
    std::vector<NodeValue> m_values;
    QTime m_invalidTime;
};

#endif // REST_NODE_BASE_H
