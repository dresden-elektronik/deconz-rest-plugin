#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMimeData>
#include <QUrl>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
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
    void dragEnterEvent(QDragEnterEvent *event) override;
//    void dragLeaveEvent(QDragLeaveEvent *event) override;
//    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showMessage(const QString &text);

    DeviceWidget *q = nullptr;
    DDF_Editor *editor;
    bool initPos = false;
};

DDF_EditorDialog::DDF_EditorDialog(DeviceWidget *parent) :
    QMainWindow(parent)
{
    q = parent;
    editor = new DDF_Editor(DeviceDescriptions::instance(), this);
    setCentralWidget(editor);

    connect(editor, &DDF_Editor::windowTitleChanged, this, &DDF_EditorDialog::setWindowTitle);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    //fileMenu->addAction(tr("&New"));

    QAction *open = fileMenu->addAction(tr("&Open"));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, parent, &DeviceWidget::openDDF);

    QAction *save = fileMenu->addAction(tr("&Save"));
    save->setShortcut(QKeySequence::Save);
    connect(save, &QAction::triggered, parent, &DeviceWidget::saveDDF);

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
    setAcceptDrops(true);
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

        int w = qMin(1200, geoWin.width() - 20);
        int h = qMin(768, geoWin.height() - 20);

        move(geoWin.x() + (geoWin.width() - w) / 4,
             geoWin.y() + (geoWin.height() - h) / 4);

        setGeometry(geoWin.x() + (geoWin.width() - w) / 4,
                    geoWin.y() + (geoWin.height() - h) / 4,
                    w, h);
    }
}

void DDF_EditorDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        const auto urls = event->mimeData()->urls();
        const auto &url = urls.first();

        if (url.scheme() == QLatin1String("file") && url.path().endsWith(QLatin1String(".json")))
        {
            event->accept();
        }
    }

    auto *m = event->mimeData();
    const auto fmts = m->formats();

    for (const auto &fmt : fmts)
    {
        DBG_Printf(DBG_INFO, "Mime-format: %s\nMime-data: %s\n", qPrintable(fmt), qPrintable(m->data(fmt)));

    }
}

void DDF_EditorDialog::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls() && editor)
    {
        const auto urls = event->mimeData()->urls();
        const auto &url = urls.first();

        if (url.scheme() == QLatin1String("file") && url.path().endsWith(QLatin1String(".json")))
        {
            auto *dd = DeviceDescriptions::instance();
            auto ddf = dd->load(url.path());
            if (ddf.isValid())
            {
                editor->setDDF(ddf);
            }

            event->accept();
        }
    }
}

class DeviceWidgetPrivat
{
public:
    DDF_EditorDialog *ddfWindow = nullptr;
    DeviceContainer *devices = nullptr;

    deCONZ::Address curNode;

    uint reloadIter = 0;
    QTimer *reloadTimer = nullptr;
};

DeviceWidget::DeviceWidget(DeviceContainer &devices, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DeviceWidget),
    d(new DeviceWidgetPrivat)
{
    d->devices = &devices;
    ui->setupUi(this);
    setWindowTitle(tr("Control"));

    d->reloadTimer = new QTimer(this);
    d->reloadTimer->setSingleShot(true);
    connect(d->reloadTimer, &QTimer::timeout, this, &DeviceWidget::reloadTimerFired);

    connect(ui->enablePermitJoinButton, &QPushButton::clicked, this, &DeviceWidget::enablePermitJoin);
    connect(ui->disablePermitJoinButton, &QPushButton::clicked, this, &DeviceWidget::disablePermitJoin);

    if      (DEV_TestStrict())  { ui->ddfStrictRadioButton->setChecked(true); }
    else if (DEV_TestManaged()) { ui->ddfNormalRadioButton->setChecked(true); }
    else                        { ui->ddfFilteredRadioButton->setChecked(true); }

    const QStringList filter = DeviceDescriptions::instance()->enabledStatusFilter();

    ui->ddfFilterBronzeCheckBox->setChecked(filter.contains("Bronze"));
    ui->ddfFilterSilverCheckBox->setChecked(filter.contains("Silver"));
    ui->ddfFilterGoldCheckBox->setChecked(filter.contains("Gold"));

    connect(ui->ddfFilteredRadioButton, &QRadioButton::clicked, this, &DeviceWidget::enableDDFHandlingChanged);
    connect(ui->ddfNormalRadioButton, &QRadioButton::clicked, this, &DeviceWidget::enableDDFHandlingChanged);
    connect(ui->ddfStrictRadioButton, &QRadioButton::clicked, this, &DeviceWidget::enableDDFHandlingChanged);

    connect(ui->ddfFilterBronzeCheckBox, &QCheckBox::clicked, this, &DeviceWidget::enableDDFHandlingChanged);
    connect(ui->ddfFilterSilverCheckBox, &QCheckBox::clicked, this, &DeviceWidget::enableDDFHandlingChanged);
    connect(ui->ddfFilterGoldCheckBox, &QCheckBox::clicked, this, &DeviceWidget::enableDDFHandlingChanged);
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

void DeviceWidget::openDDF()
{
    QString dir = deCONZ::getStorageLocation(deCONZ::DdfUserLocation);
    QString path = QFileDialog::getOpenFileName(d->ddfWindow,
                                                tr("Open DDF file"),
                                                dir,
                                                tr("DDF files (*.json)"));

    if (path.isEmpty())
    {
        return;
    }

    auto *dd = DeviceDescriptions::instance();
    auto ddf = dd->load(path);
    if (!ddf.isValid())
    {
        d->ddfWindow->showMessage(tr("Failed to open %1").arg(path));
        return;
    }

    d->ddfWindow->editor->setDDF(ddf);
}

void DeviceWidget::saveDDF()
{
    DeviceDescription ddf = d->ddfWindow->editor->ddf();

    QFileInfo fi(ddf.path);

    if (ddf.manufacturerNames.empty() || ddf.modelIds.empty())
    {
        d->ddfWindow->showMessage(tr("Device model ID and manufacturer must be set"));
        return;
    }

    if (ddf.path.isEmpty() || !fi.isWritable())
    {
        saveAsDDF();
        return;
    }

    if (ddf.product.isEmpty())
    {
        ddf.product = ddf.modelIds.first();
    }

    QFile f(ddf.path);

    if (!f.open(QIODevice::WriteOnly))
    {
        d->ddfWindow->showMessage(tr("Failed to write %1").arg(ddf.path));
        return;
    }

    const QString ddfJson = DDF_ToJsonPretty(ddf);
    f.write(ddfJson.toUtf8());

    d->ddfWindow->editor->updateDDFHash();
    d->ddfWindow->showMessage(tr("DDF saved to %1").arg(ddf.path));
}

void DeviceWidget::saveAsDDF()
{
    DeviceDescription ddf = d->ddfWindow->editor->ddf();

    if (ddf.manufacturerNames.empty() || ddf.modelIds.empty())
    {
        d->ddfWindow->showMessage(tr("Device model ID and manufacturer must be set"));
        return;
    }

    if (ddf.product.isEmpty())
    {
        ddf.product = ddf.modelIds.first();
    }

    QString saveFilePath = ddf.path;

    if (saveFilePath.isEmpty())
    {
        QString fileName = ddf.product;
        for (auto &ch : fileName)
        {
            if (ch == QLatin1Char(' ') || ch.unicode() > 'z')
            {
                ch = QLatin1Char('_');
            }
        }
        QString dir = deCONZ::getStorageLocation(deCONZ::DdfUserLocation);
        saveFilePath = QString("%1/%2.json").arg(dir, fileName.toLower());
    }

    QString path = QFileDialog::getSaveFileName(d->ddfWindow,
                                                tr("Save DDF file as"),
                                                saveFilePath,
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

    ddf.path = path;
    const QString ddfJson = DDF_ToJsonPretty(ddf);
    f.write(ddfJson.toUtf8());

    d->ddfWindow->editor->setDDF(ddf);
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

    d->ddfWindow->showMessage(tr("DDF reloaded for devices"));
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
    QStringList filter;

    if (ui->ddfFilteredRadioButton->isChecked())
    {
        DEV_SetTestManaged(0);

        filter.clear();
        if (ui->ddfFilterBronzeCheckBox->isChecked()) { filter.append("Bronze"); }
        if (ui->ddfFilterSilverCheckBox->isChecked()) { filter.append("Silver"); }
        if (ui->ddfFilterGoldCheckBox->isChecked()) { filter.append("Gold"); }
    }
    else if (ui->ddfNormalRadioButton->isChecked())
    {
        DEV_SetTestManaged(1);

        filter.append("Bronze");
        filter.append("Silver");
        filter.append("Gold");
    }
    else if (ui->ddfStrictRadioButton->isChecked())
    {
        DEV_SetTestManaged(2);

        filter.append("Bronze");
        filter.append("Silver");
        filter.append("Gold");
    }

    if (filter != DeviceDescriptions::instance()->enabledStatusFilter())
    {
        DeviceDescriptions::instance()->setEnabledStatusFilter(filter);

        QSettings config(deCONZ::getStorageLocation(deCONZ::ConfigLocation), QSettings::IniFormat);

        config.setValue("ddf-filter/bronze", ui->ddfFilterBronzeCheckBox->isChecked() ? 1 : 0);
        config.setValue("ddf-filter/silver", ui->ddfFilterSilverCheckBox->isChecked() ? 1 : 0);
        config.setValue("ddf-filter/gold", ui->ddfFilterGoldCheckBox->isChecked() ? 1 : 0);
    }

    // reload all Devices to bring state machine in correct state
    d->reloadIter = 0;
    d->reloadTimer->start(1000);
}

void DeviceWidget::reloadTimerFired()
{
    if (d->reloadIter >= d->devices->size())
    {
        return;
    }

    Device *device = d->devices->at(d->reloadIter).get();
    device->handleEvent(Event(RDevices, REventDDFReload, 0, device->key()));
    d->reloadIter++;
    d->reloadTimer->start(1000);
}
