/*
 * Copyright (c) 2013-2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QAction>
#include <QLabel>
#include <QNetworkInterface>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "de_web_widget.h"
#include "ui_de_web_widget.h"

QAction *readBindingTableAction = nullptr;

/*! Constructor. */
DeRestWidget::DeRestWidget(QWidget *parent, DeRestPlugin *_plugin) :
    QDialog(parent),
    ui(new Ui::DeWebWidget),
    plugin(_plugin)
{
    ui->setupUi(this);
    setWindowTitle(tr("DE REST-API"));
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

    quint16 httpPort = apsCtrl ? deCONZ::ApsController::instance()->getParameter(deCONZ::ParamHttpPort) : 0;

    ui->ipAddressesLabel->setTextFormat(Qt::RichText);
    ui->ipAddressesLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->ipAddressesLabel->setOpenExternalLinks(true);
    ui->gitCommitLabel->setText(QLatin1String(GIT_COMMMIT));

    QString str;
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();

    QList<QNetworkInterface>::Iterator ifi = ifaces.begin();
    QList<QNetworkInterface>::Iterator ifend = ifaces.end();

    for (; ifi != ifend; ++ifi)
    {
        QString name = ifi->humanReadableName();

        // filter
        if (name.contains("br-", Qt::CaseInsensitive) ||
            name.contains("docker", Qt::CaseInsensitive) ||
            name.contains("vm", Qt::CaseInsensitive) ||
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
                QString url = QString("http://%1:%2").arg(a.toString()).arg(httpPort);

                str.append("<b>");
                str.append(ifi->humanReadableName());
                str.append("</b>&nbsp;&nbsp;&nbsp;&nbsp;");
                str.append(QString("<a href=\"%1\">%2</a><br/>").arg(url).arg(url));
            }
        }
    }

    if (httpPort == 0)
    {
        str = tr("No HTTP server is running");
    }

    ui->ipAddressesLabel->setText(str);

    //
    connect(deCONZ::ApsController::instance(), &deCONZ::ApsController::nodeEvent, this, &DeRestWidget::nodeEvent);

    // keyboard shortcuts
    readBindingTableAction = new QAction(tr("Read binding table"), this);
    readBindingTableAction->setShortcut(Qt::CTRL + Qt::Key_B);
    readBindingTableAction->setProperty("type", "node-action");
    readBindingTableAction->setProperty("actionid", "read-binding-table");
    readBindingTableAction->setEnabled(m_selectedNodeAddress.hasExt());
    connect(readBindingTableAction, &QAction::triggered, this, &DeRestWidget::readBindingTableTriggered);
    addAction(readBindingTableAction);
}

/*! Deconstructor. */
DeRestWidget::~DeRestWidget()
{
    Q_ASSERT(ui);
    delete ui;
    ui = nullptr;
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

void DeRestWidget::readBindingTableTriggered()
{
    if (m_selectedNodeAddress.hasExt())
    {
        auto *restNode = dynamic_cast<RestNodeBase*>(plugin->d->getLightNodeForAddress(m_selectedNodeAddress));

        if (!restNode)
        {
            restNode = dynamic_cast<RestNodeBase*>(plugin->d->getSensorNodeForAddress(m_selectedNodeAddress));
        }

        if (restNode)
        {
            restNode->setMgmtBindSupported(true);
            plugin->d->readBindingTable(restNode, 0);
        }
    }
}

void DeRestWidget::nodeEvent(const deCONZ::NodeEvent &event)
{
    if (event.node() && event.event() == deCONZ::NodeEvent::NodeSelected)
    {
        m_selectedNodeAddress = event.node()->address();
        readBindingTableAction->setEnabled(m_selectedNodeAddress.hasExt());
    }
    else if (event.event() == deCONZ::NodeEvent::NodeDeselected)
    {
        m_selectedNodeAddress = {};
        readBindingTableAction->setEnabled(false);
    }
}

void DeRestWidget::showEvent(QShowEvent *)
{
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

    if (!apsCtrl)
    {
        return;
    }

    QByteArray sec0 = apsCtrl->getParameter(deCONZ::ParamSecurityMaterial0);

    if (!sec0.isEmpty())
    {
        QByteArray installCode;
        for (int i = 0; i < 4; i++)
        {
            installCode += sec0.mid(i * 4, 4);
            if (i < 3) { installCode += ' '; }
        }
        ui->labelInstallCode->setText(installCode);
    }
    else
    {
        ui->labelInstallCode->setText(tr("not available"));
    }

}
