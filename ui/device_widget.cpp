#include <QDesktopServices>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QUrl>
#include <QStatusBar>
#include <deconz/node.h>
#include <deconz/node_event.h>
#include <deconz/dbg_trace.h>
#include "device_widget.h"
#include "device_descriptions.h"
#include "ddf_editor.h"
#include "ddf_treeview.h"
#include "device.h"
#include "device_widget.h"
#include "event.h"
#include "ui_device_widget.h"
#include "rest_devices.h"

class DDF_EditorDialog: public QMainWindow
{
public:
    DDF_EditorDialog(DeviceWidget *parent);

    void showEvent(QShowEvent *t) override;
    void showMessage(const QString &text);

    DDF_Editor *editor;
    bool initPos = false;
};

DDF_EditorDialog::DDF_EditorDialog(DeviceWidget *parent) :
    QMainWindow(parent)
{
    editor = new DDF_Editor(DeviceDescriptions::instance(), this);
    setCentralWidget(editor);

    connect(editor, &DDF_Editor::windowTitleChanged, this, &DDF_EditorDialog::setWindowTitle);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"));
    fileMenu->addAction(tr("&Open"));
    fileMenu->addAction(tr("&Save"));

    QAction *saveAs = fileMenu->addAction(tr("&Save as"));
    saveAs->setShortcut(QKeySequence::SaveAs);
    connect(saveAs, &QAction::triggered, parent, &DeviceWidget::saveAsDDF);

    QAction *hotReload = fileMenu->addAction(tr("&Hot reload"));
    hotReload->setShortcut(tr("Ctrl+R"));
    connect(hotReload, &QAction::triggered, parent, &DeviceWidget::hotReload);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *docAction = helpMenu->addAction(tr("DDF documentation"));
    connect(docAction, &QAction::triggered, [](){
        QDesktopServices::openUrl(QUrl("https://dresden-elektronik.github.io/deconz-dev-doc/modules/ddf"));
    });

    setWindowTitle(tr("DDF Editor"));

}

void DDF_EditorDialog::showMessage(const QString &text)
{
    statusBar()->showMessage(text, 10000);
}

void DDF_EditorDialog::showEvent(QShowEvent *)
{
    // workaround to center over mainwindow on first open
    if (!initPos)
    {
        initPos = true;
        auto geoWin = qApp->activeWindow()->geometry();
//        auto geo0 = geometry();
        int w = qMin(1380, geoWin.width() - 20);
        int h = qMin(768, geoWin.height() - 20);

        move(geoWin.x() + (geoWin.width() - w) / 4,
             geoWin.y() + (geoWin.height() - h) / 4);

        setGeometry(geoWin.x() + (geoWin.width() - w) / 4,
                    geoWin.y() + (geoWin.height() - h) / 4,
                    w, h);
    }
}

class DeviceWidgetPrivat
{
public:
    DDF_EditorDialog *ddfWindow = nullptr;
    DeviceContainer *devices = nullptr;

    deCONZ::Address curNode;
};

DeviceWidget::DeviceWidget(DeviceContainer &devices, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DeviceWidget),
    d(new DeviceWidgetPrivat)
{
    d->devices = &devices;
    ui->setupUi(this);
    setWindowTitle(tr("Control"));

    connect(ui->enablePermitJoinButton, &QPushButton::clicked, this, &DeviceWidget::enablePermitJoin);
    connect(ui->disablePermitJoinButton, &QPushButton::clicked, this, &DeviceWidget::disablePermitJoin);

    ui->enableDDFCheckBox->setChecked(DEV_TestManaged());
    connect(ui->enableDDFCheckBox, &QCheckBox::stateChanged, this, &DeviceWidget::enableDDFHandlingChanged);
}

DeviceWidget::~DeviceWidget()
{
    delete ui;
    delete d;
}

void DeviceWidget::handleEvent(const Event &event)
{
    if (event.what()[0] == 'e') // filter: "event/*"
    {
        if (event.what() == REventPermitjoinEnabled)
        {
            ui->permitJoinStackedWidget->setCurrentWidget(ui->permitJoinEnabledPage);
            ui->permitJoinRemainingTimeLabel->setText(QString::number(event.num()));
        }
        else if (event.what() == REventPermitjoinRunning)
        {
            ui->permitJoinRemainingTimeLabel->setText(QString::number(event.num()));
        }
        else if (event.what() == REventPermitjoinDisabled)
        {
            ui->permitJoinStackedWidget->setCurrentWidget(ui->permitJoinDisabledPage);
        }
    }
}

void DeviceWidget::nodeEvent(const deCONZ::NodeEvent &event)
{
    if (event.event() == deCONZ::NodeEvent::NodeDeselected)
    {
        ui->uniqueIdLabel->setText("No node selected");
        d->curNode = {};
    }
    else if (!event.node())
    {

    }
    else if (event.event() == deCONZ::NodeEvent::NodeSelected)
    {
        d->curNode = event.node()->address();

        Device *device = DEV_GetDevice(*d->devices, d->curNode.ext());
        if (!device)
        {
            ui->uniqueIdLabel->setText("No device");
            return;
        }

        ui->uniqueIdLabel->setText(device->item(RAttrUniqueId)->toString());
    }
#if DECONZ_LIB_VERSION >= 0x011003
    else if (event.event() == deCONZ::NodeEvent::EditDeviceDDF)
    {
        d->curNode = event.node()->address();
        editDDF();
    }
#endif
}

void DeviceWidget::editDDF()
{
    Device *device = DEV_GetDevice(*d->devices, d->curNode.ext());
    if (!device)
    {
        return;
    }

    if (!d->ddfWindow)
    {
        d->ddfWindow = new DDF_EditorDialog(this);
        d->ddfWindow->hide();
    }

    if (d->ddfWindow)
    {
        const DeviceDescription &ddf = DeviceDescriptions::instance()->get(device);
        d->ddfWindow->editor->setDDF(ddf);
        d->ddfWindow->show();
        d->ddfWindow->raise();
    }
}

void DeviceWidget::saveAsDDF()
{
    const DeviceDescription &ddf = d->ddfWindow->editor->ddf();

    if (ddf.product.isEmpty())
    {
        d->ddfWindow->showMessage(tr("Device product must be set"));
        return;
    }

    QString fileName = ddf.product;
    for (auto &ch : fileName)
    {
        if (ch == QLatin1Char(' ') || ch.unicode() > 'z')
        {
            ch = QLatin1Char('_');
        }
    }

    QString dir = deCONZ::getStorageLocation(deCONZ::DdfUserLocation);
    QString path = QFileDialog::getSaveFileName(d->ddfWindow,
                                                tr("Save DDF file as"),
                                                QString("%1/%2.json").arg(dir, fileName.toLower()),
                                                tr("DDF files (*.json)"));

    if (path.isEmpty())
    {
        return;
    }

    QFile f(path);

    if (!f.open(QIODevice::WriteOnly))
    {
        d->ddfWindow->showMessage(tr("Failed to write %1").arg(path));
        return;
    }

    const QString ddfJson = DDF_ToJsonPretty(ddf);
    f.write(ddfJson.toUtf8());

    d->ddfWindow->showMessage(tr("DDF save to %1").arg(path));
}

void DeviceWidget::hotReload()
{
    const DeviceDescription &ddf = d->ddfWindow->editor->ddf();
    if (!ddf.isValid())
    {
        return;
    }
    auto *dd = DeviceDescriptions::instance();
    dd->put(ddf);

    for (const std::unique_ptr<Device> &dev : *d->devices)
    {
        const auto &ddf0 = dd->get(&*dev);

        if (ddf0.handle == ddf.handle)
        {
            DBG_Printf(DBG_INFO, "Hot reload device: %s\n", dev->item(RAttrUniqueId)->toCString());
            dev->handleEvent(Event(RDevices, REventDDFReload, 0, dev->key()));
        }
    }
}

void DeviceWidget::enablePermitJoin()
{
    emit permitJoin(60);
}

void DeviceWidget::disablePermitJoin()
{
    emit permitJoin(0);
}

void DeviceWidget::enableDDFHandlingChanged()
{
    DEV_SetTestManaged(ui->enableDDFCheckBox->isChecked());
}

