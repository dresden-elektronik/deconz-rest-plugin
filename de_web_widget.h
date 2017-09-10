/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DE_WEB_WIDGET_H
#define DE_WEB_WIDGET_H

#include <QDialog>

namespace Ui {
class DeWebWidget;
}

class DeRestWidget : public QDialog
{
    Q_OBJECT
    
public:
    explicit DeRestWidget(QWidget *parent);
    ~DeRestWidget();
    bool pluginActive() const;

public Q_SLOTS:

private:
    void showEvent(QShowEvent *);
    Ui::DeWebWidget *ui;
};

#endif // DE_WEB_WIDGET_H
