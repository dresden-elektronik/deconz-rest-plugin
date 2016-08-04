/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QNetworkInterface>
#include "de_web_plugin.h"
#include "de_web_widget.h"
#include "ui_de_web_widget.h"

/*! Constructor. */
DeRestWidget::DeRestWidget(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DeWebWidget)
{
    ui->setupUi(this);
    setWindowTitle(tr("DE Web App"));

    connect(ui->refreshAllButton, SIGNAL(clicked()),
            this, SIGNAL(refreshAllClicked()));

    connect(ui->changeChannelButton, SIGNAL(clicked()),
            this, SLOT(onChangeChannelClicked()));

    ui->changeChannelButton->hide();
    ui->channelSpinBox->hide();

    QString str;
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();

    QList<QNetworkInterface>::Iterator ifi = ifaces.begin();
    QList<QNetworkInterface>::Iterator ifend = ifaces.end();

    for (; ifi != ifend; ++ifi)
    {
        QString name = ifi->humanReadableName();

        // filter
        if (name.contains("vm", Qt::CaseInsensitive) ||
            name.contains("virtual", Qt::CaseInsensitive) ||
            name.contains("loop", Qt::CaseInsensitive))
        {
            continue;
        }

        QList<QNetworkAddressEntry> addr = ifi->addressEntries();

        QList<QNetworkAddressEntry>::Iterator i = addr.begin();
        QList<QNetworkAddressEntry>::Iterator end = addr.end();

        for (; i != end; ++i)
        {
            QHostAddress a = i->ip();
            if (a.protocol() == QAbstractSocket::IPv4Protocol)
            {
                str.append("<b>");
                str.append(ifi->humanReadableName());
                str.append("</b>&nbsp;&nbsp;&nbsp;&nbsp;");
                str.append(a.toString());
                str.append("<br/>");
            }
        }

    }

    ui->ipAddressesLabel->setText(str);
}

/*! Deconstructor. */
DeRestWidget::~DeRestWidget()
{
    delete ui;
}

/*! Returns true if the plugin is active. */
bool DeRestWidget::pluginActive() const
{
    if (ui)
    {
        return ui->pluginActiveCheckBox->isChecked();
    }
    return false;
}

/*! Forward user input. */
void DeRestWidget::onChangeChannelClicked()
{
    emit changeChannelClicked((quint8)ui->channelSpinBox->value());
}
