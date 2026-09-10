#include <QCompleter>
#include <QCryptographicHash>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QLineEdit>
#include <QMimeData>
#include <QUrlQuery>
#include <QTimer>
#include <deconz/dbg_trace.h>
#include "device_descriptions.h"
#include "ddf_itemlist.h"
#include "ddf_treeview.h"
#include "ddf_editor.h"
#include "ddf_itemeditor.h"
#include "product_match.h"
#include "rest_devices.h"
#include "ui_ddf_editor.h"

enum EditorState
{
    StateInit,
    StateLoad,
    StateEdit
};

class DDF_EditorPrivate
{
public:
    EditorState state = StateInit;
    DeviceDescriptions *dd;
    DeviceDescription ddf;
    QByteArray ddfOrigSha1;
    QTimer *checkDDFChangedTimer;
//    QRegExpValidator *u16Validator;
    uint curSubDevice = 0;
    uint curItem = 0;
};

DDF_Editor::DDF_Editor(DeviceDescriptions *dd, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DDF_Editor)
{
    ui->setupUi(this);

    d = new DDF_EditorPrivate;
    d->dd = dd;
    d->checkDDFChangedTimer = new QTimer(this);
    d->checkDDFChangedTimer->setSingleShot(true);
    connect(d->checkDDFChangedTimer, &QTimer::timeout, this, &DDF_Editor::checkDDFChanged);

    connect(ui->ddfTreeView, &DDF_TreeView::itemSelected, this, &DDF_Editor::itemSelected);
    connect(ui->ddfTreeView, &DDF_TreeView::addItem, this, &DDF_Editor::addItem);
    connect(ui->ddfTreeView, &DDF_TreeView::addSubDevice, this, &DDF_Editor::addSubDevice);
    connect(ui->ddfTreeView, &DDF_TreeView::subDeviceSelected, this, &DDF_Editor::subDeviceSelected);
    connect(ui->ddfTreeView, &DDF_TreeView::deviceSelected, this, &DDF_Editor::deviceSelected);
    connect(ui->ddfTreeView, &DDF_TreeView::removeItem, this, &DDF_Editor::removeItem);
    connect(ui->ddfTreeView, &DDF_TreeView::removeSubDevice, this, &DDF_Editor::removeSubDevice);

    connect(ui->editItem, &DDF_ItemEditor::itemChanged, this, &DDF_Editor::itemChanged);

    {
        QStringList wordlist;
        const auto &subDevices = dd->getSubDevices();

        for (const auto &sub : subDevices)
        {
            wordlist.push_back(sub.type);
        }

        QCompleter *subDeviceCompleter = new QCompleter(wordlist, this);
        ui->subDeviceTypeInput->setCompleter(subDeviceCompleter);
    }

    connect(ui->subDeviceTypeInput, &TextLineEdit::valueChanged, this, &DDF_Editor::subDeviceInputChanged);
    connect(ui->subDeviceUniqueIdInput, &TextLineEdit::valueChanged, this, &DDF_Editor::subDeviceInputChanged);

    ui->devVendorInput->setIsOptional(true);

    ui->devManufacturerNameInput->installEventFilter(this);
    ui->devModelIdInput->installEventFilter(this);
    ui->devVendorInput->installEventFilter(this);
    ui->devProductInput->installEventFilter(this);

    connect(ui->devManufacturerNameInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devModelIdInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devVendorInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devProductInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devSleeperCheckBox, &QCheckBox::stateChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devStatusComboBox, &QComboBox::currentTextChanged, this, &DDF_Editor::deviceChanged);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &DDF_Editor::tabChanged);

    connect(ui->tabBindings, &DDF_BindingEditor::bindingsChanged, this, &DDF_Editor::bindingsChanged);
}

DDF_Editor::~DDF_Editor()
{
    delete ui;
    delete d;
}

// to show items alphabetically ordered, sort them by name
void DDF_SortItems(DeviceDescription &ddf)
{
    for (DeviceDescription::SubDevice &sub : ddf.subDevices)
    {
        std::sort(sub.items.begin(), sub.items.end(), [](const DeviceDescription::Item &a, const DeviceDescription::Item &b)
        {
            return a.name < b.name;
        });
    }
}

void DDF_Editor::setDDF(const DeviceDescription &ddf)
{
    if (ddf.manufacturerNames.isEmpty() || ddf.modelIds.isEmpty())
    {
        return;
    }

    d->state = StateLoad;
    d->ddf = ddf;

    if (d->ddf.product.isEmpty())
    {
        d->ddf.product = d->ddf.modelIds.first();
    }

    DDF_SortItems(d->ddf);
    updateDDFHash();

    QStringList mfNames = ddf.manufacturerNames;
    for (QString &mf : mfNames)
    {
        mf = d->dd->constantToString(mf);
    }

    ui->devManufacturerNameInput->setInputText(mfNames.join(QLatin1Char(',')));
    ui->devModelIdInput->setInputText(d->ddf.modelIds.join(QLatin1Char(',')));
    ui->devVendorInput->setInputText(d->ddf.vendor);
    ui->devProductInput->setInputText(d->ddf.product);
    ui->devSleeperCheckBox->setChecked(d->ddf.sleeper == 1);
    ui->devStatusComboBox->setCurrentText(ddf.status);

    ui->itemListView->update(d->dd);
    ui->ddfTreeView->setDDF(d->ddf);
    ui->tabBindings->setBindings(d->ddf.bindings);

    checkDDFChanged(); // to set window title
    d->state = StateEdit;

    deviceChanged();
}

void DDF_Editor::previewDDF(const DeviceDescription &ddf)
{
    ui->ddfJsonDoc->setPlainText(DDF_ToJsonPretty(ddf));
}

void DDF_Editor::updateDDFHash()
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(DDF_ToJsonPretty(d->ddf).toUtf8());
    d->ddfOrigSha1 = hash.result();
    startCheckDDFChanged();
}

const DeviceDescription &DDF_Editor::ddf() const
{
    return d->ddf;
}

void DDF_Editor::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    ui->tabWidget->setCurrentWidget(ui->tabItems);
}

bool DDF_Editor::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::DragEnter)
    {
        auto *input = dynamic_cast<TextLineEdit*>(object);

        if (!input)
        {
            return false;
        }

        QDragEnterEvent *e = static_cast<QDragEnterEvent*>(event);

        if (!e->mimeData()->hasUrls())
        {
            return false;
        }

        const auto urls = e->mimeData()->urls();
        const auto url = urls.first();

        if (url.scheme() == QLatin1String("zclattr"))
        {
            QUrlQuery urlQuery(url);

            if (urlQuery.hasQueryItem(QLatin1String("val")) && !urlQuery.queryItemValue(QLatin1String("val")).isEmpty())
            {
                e->accept();
                return true;
            }
        }
    }
    else if (event->type() == QEvent::Drop)
    {
        auto *input = dynamic_cast<TextLineEdit*>(object);
        if (!input)
        {
            return false;
        }

        QDropEvent *e = static_cast<QDropEvent*>(event);

        if (!e->mimeData()->hasUrls())
        {
            return false;
        }

        const auto urls = e->mimeData()->urls();
        const auto &url = urls.first();

        if (url.scheme() == QLatin1String("zclattr"))
        {
            QUrlQuery urlQuery(url);

            if (urlQuery.hasQueryItem(QLatin1String("val")))
            {
                QString val = urlQuery.queryItemValue(QLatin1String("val"));
                if (!val.isEmpty())
                {
                    input->setInputText(val);
                }
            }
        }
        return true;
    }

    return false;
}

void DDF_Editor::itemSelected(uint subDevice, uint item)
{
    if (subDevice >= d->ddf.subDevices.size())
    {
        return;
    }

    auto &sub = d->ddf.subDevices[subDevice];

    if (item >= sub.items.size())
    {
        return;
    }

    const DeviceDescription::Item &ddfItem = sub.items[item];

    if (!ddfItem.isValid())
    {
        return;
    }

    d->curSubDevice = subDevice;
    d->curItem = item;

    if (ddfItem.isManaged)
    {
        if (ddfItem.description.isEmpty())
        {
            const auto &genItem = d->dd->getGenericItem(ddfItem.descriptor.suffix);
            ui->managedItemDescription->setText(genItem.description);
        }
        else
        {
            ui->managedItemDescription->setText(ddfItem.description);
        }
        ui->managedItemLabel->setText(tr("Item: %1").arg(QLatin1String(ddfItem.name.c_str())));
        ui->editStackedWidget->setCurrentWidget(ui->managedItem);
    }
    else
    {
        ui->editItem->setItem(ddfItem, d->dd);
        ui->editStackedWidget->setCurrentWidget(ui->editItem);
    }
}

void DDF_Editor::itemChanged()
{
    if (d->curSubDevice >= d->ddf.subDevices.size())
    {
        return;
    }

    if (d->curItem >= d->ddf.subDevices[d->curSubDevice].items.size())
    {
        return;
    }

    d->ddf.subDevices[d->curSubDevice].items[d->curItem] = ui->editItem->item();
    startCheckDDFChanged();
}

void DDF_Editor::subDeviceSelected(uint subDevice)
{
    if (d->ddf.subDevices.size() <= subDevice)
    {
        return;
    }

    d->curSubDevice = d->ddf.subDevices.size(); // prevent field change

    const DeviceDescription::SubDevice &sub = d->ddf.subDevices[subDevice];

    ui->subDeviceTypeInput->setInputText(d->dd->constantToString(sub.type));
    ui->subDeviceUniqueIdInput->setInputText(sub.uniqueId.join(QLatin1Char('-')));
    d->curSubDevice = subDevice;

    ui->editStackedWidget->setCurrentWidget(ui->editSubdevice);
}

void DDF_Editor::deviceSelected()
{
    ui->editStackedWidget->setCurrentWidget(ui->editDevice);
}

void DDF_Editor::addItem(uint subDevice, const QString &suffix)
{

    if (!d->ddf.isValid() || d->ddf.subDevices.size() <= subDevice)
    {
        return;
    }

    DeviceDescription::SubDevice &sub = d->ddf.subDevices[subDevice];

    BufString<64> bSuffix(qPrintable(suffix));

    {
        const auto i = std::find_if(sub.items.cbegin(), sub.items.cend(),
                                    [&bSuffix](const DeviceDescription::Item &i) {  return i.name == bSuffix; });

        if (i != sub.items.cend())
        {
            return; // already
        }
    }

    const DDF_Items &genItems = d->dd->genericItems();

    const auto i = std::find_if(genItems.cbegin(), genItems.cend(),
                                    [&bSuffix](const DeviceDescription::Item &i) { return i.name == bSuffix; });

    if (i != genItems.cend())
    {
        sub.items.push_back(*i);
        DDF_SortItems(d->ddf);

        ui->ddfTreeView->setDDF(d->ddf);
    }
    startCheckDDFChanged();
}

void DDF_Editor::addSubDevice(const QString &name)
{
//    {
//        const auto i = std::find_if(d->ddf.subDevices.cbegin(), d->ddf.subDevices.cend(), [&](const auto &sub)
//        {
//            return d->dd->constantToString(sub.type) == name;
//        });

//        if (i != d->ddf.subDevices.cend())
//        {
//            DBG_Printf(DBG_INFO, "%s already exists\n", qPrintable(name));
//            return;
//        }
//    }

    const auto &subDevices = d->dd->getSubDevices();

    const auto i = std::find_if(subDevices.cbegin(), subDevices.cend(), [&](const auto &sub)
    {
        return sub.name == name;
    });

    if (i != subDevices.cend() && isValid(*i))
    {
        DeviceDescription::SubDevice sub;

        sub.type = i->type;
        sub.restApi = i->restApi;
        sub.uniqueId = i->uniqueId;

        auto items = i->items;

        // default items for all sub-devices
        items.push_back(RAttrId);
        items.push_back(RAttrLastSeen);
        items.push_back(RAttrLastAnnounced);
        items.push_back(RAttrManufacturerName);
        items.push_back(RAttrModelId);
        items.push_back(RAttrName);
        items.push_back(RAttrSwVersion);
        items.push_back(RAttrType);
        items.push_back(RAttrUniqueId);

        std::sort(items.begin(), items.end(), [](const auto *a, const auto *b)
        {
            return strcmp(a, b) < 0;
        });

        for (const auto *suffix : items)
        {
            auto item = d->dd->getGenericItem(suffix);
            if (item.isValid())
            {
                sub.items.push_back(item);
            }
        }

        d->ddf.subDevices.push_back(sub);
        ui->ddfTreeView->setDDF(d->ddf);

        d->curItem = 0;
        subDeviceSelected(d->ddf.subDevices.size() - 1);
        startCheckDDFChanged();
    }
}

void DDF_Editor::deviceChanged()
{
    if (d->state != StateEdit)
    {
        return;
    }

    QStringList mfNames = ui->devManufacturerNameInput->text().split(QLatin1Char(','), SKIP_EMPTY_PARTS);
    for (QString &mf : mfNames)
    {
        mf = d->dd->stringToConstant(mf);
    }

    d->ddf.manufacturerNames = mfNames;
    d->ddf.status = ui->devStatusComboBox->currentText();
    d->ddf.vendor = ui->devVendorInput->text();
    d->ddf.product = ui->devProductInput->text();
    d->ddf.modelIds = ui->devModelIdInput->text().split(QLatin1Char(','), SKIP_EMPTY_PARTS);
    d->ddf.sleeper = ui->devSleeperCheckBox->isChecked();

    if (!d->ddf.vendor.isEmpty())
    {
        ui->devManufacturerNameLabel->setText(d->ddf.vendor);
    }
    else if (!mfNames.isEmpty())
    {
        ui->devManufacturerNameLabel->setText(d->dd->constantToString(mfNames.first()));
    }

    if (d->ddf.modelIds.size() > 0)
    {
        ui->devModelIdLabel->setText(d->ddf.modelIds.first());
    }
    else
    {
        ui->devModelIdLabel->clear();
    }
    startCheckDDFChanged();
}

void DDF_Editor::tabChanged()
{
    if (ui->tabWidget->currentWidget() == ui->tabPreview)
    {
        previewDDF(d->ddf);
    }
    else if (ui->tabWidget->currentWidget() == ui->tabBindings)
    {

    }
}

void DDF_Editor::removeItem(uint subDevice, uint item)
{
    if (subDevice >= d->ddf.subDevices.size())
    {
        return;
    }

    auto &sub = d->ddf.subDevices[subDevice];
    if (item >= sub.items.size())
    {
        return;
    }

    sub.items.erase(sub.items.begin() + item);

    if (d->curItem > 0)
    {
        d->curItem--;
    }

    ui->ddfTreeView->setDDF(d->ddf);
    itemSelected(d->curSubDevice, d->curItem);
    startCheckDDFChanged();
}

void DDF_Editor::removeSubDevice(uint subDevice)
{
    if (subDevice >= d->ddf.subDevices.size())
    {
        return;
    }

    d->ddf.subDevices.erase(d->ddf.subDevices.begin() + subDevice);

    if (d->curSubDevice > 0)
    {
        d->curSubDevice--;
    }

    d->curItem = 0;
    ui->ddfTreeView->setDDF(d->ddf);
    itemSelected(d->curSubDevice, d->curItem);
    startCheckDDFChanged();
}

void DDF_Editor::subDeviceInputChanged()
{
    if (d->ddf.subDevices.size() <= d->curSubDevice)
    {
        return;
    }

    bool changed = false;
    DeviceDescription::SubDevice &sub = d->ddf.subDevices[d->curSubDevice];
    const QStringList uniqueId = ui->subDeviceUniqueIdInput->text().split(QLatin1Char('-'), SKIP_EMPTY_PARTS);
    const QString type = d->dd->stringToConstant(ui->subDeviceTypeInput->text());
    const auto &subDevices = d->dd->getSubDevices();

    auto i = std::find_if(subDevices.cbegin(), subDevices.cend(), [&type](const auto &sub){ return sub.type == type; });
    if (i == subDevices.cend())
    {
        return;
    }

    if (type != sub.type)
    {
        sub.type = type;
        sub.restApi = i->restApi;
        changed = true;
    }

    if (uniqueId.size() == i->uniqueId.size() && uniqueId != sub.uniqueId)
    {
        sub.uniqueId = uniqueId;
        changed = true;
    }

    if (changed)
    {
        ui->ddfTreeView->setDDF(d->ddf);
        startCheckDDFChanged();
    }
}

void DDF_Editor::bindingsChanged()
{
    d->ddf.bindings = ui->tabBindings->bindings();
    startCheckDDFChanged();
}

void DDF_Editor::startCheckDDFChanged()
{
    if (d->checkDDFChangedTimer->isActive())
    {
        d->checkDDFChangedTimer->stop();
    }
    d->checkDDFChangedTimer->start(200);
}

void DDF_Editor::checkDDFChanged()
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(DDF_ToJsonPretty(d->ddf).toUtf8());

    auto sha1 = hash.result();

    QLatin1Char changed(sha1 != d->ddfOrigSha1 ? '*' : ' ');

    QString path;

    if (!d->ddf.path.isEmpty())
    {
        QFileInfo fi(d->ddf.path);
        QDir dir = fi.dir();

        path = changed + dir.dirName() + '/' + fi.fileName();
    }
    else
    {
        path = changed + tr("Untitled file");
    }

    setWindowTitle(QString("%1 - DDF Editor").arg(path));
}
