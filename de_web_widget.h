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
#include <deconz.h>

namespace Ui {
class DeWebWidget;
}

class DeRestPlugin;

class DeRestWidget : public QDialog
{
    Q_OBJECT
    
public:
    explicit DeRestWidget(QWidget *parent, DeRestPlugin *_plugin);
    ~DeRestWidget();
    bool pluginActive() const;

private Q_SLOTS:
    void readBindingTableTriggered();
    void nodeEvent(const deCONZ::NodeEvent &event);

private:
    void showEvent(QShowEvent *);
    deCONZ::Address m_selectedNodeAddress;
    Ui::DeWebWidget *ui = nullptr;
    DeRestPlugin *plugin = nullptr;
};

#endif // DE_WEB_WIDGET_H
