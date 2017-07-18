/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */
#ifndef RESOURCELINKS_H
#define RESOURCELINKS_H

#include <QString>
#include <QVariantMap>
#include <vector>

/*! \class Resourcelinks

    Represents REST API resourcelinks.
 */
class Resourcelinks
{
public:
    enum State { StateNormal, StateDeleted };
    Resourcelinks();
    bool needSaveDatabase() const;
    void setNeedSaveDatabase(bool needSave);

    State state;
    QString id;
    QVariantMap data;

private:
    bool m_needSaveDatabase;
};

#endif // RESOURCELINKS_H
