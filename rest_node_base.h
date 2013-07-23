/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_NODE_BASE_H
#define REST_NODE_BASE_H

#include "deconz.h"

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
    bool isAvailable() const;
    void setIsAvailable(bool available);
    const QString &id() const;
    void setId(const QString &id);

private:
    deCONZ::Node *m_node;
    deCONZ::Address m_addr;
    QString m_id;
    bool m_available;
};

#endif // REST_NODE_BASE_H
