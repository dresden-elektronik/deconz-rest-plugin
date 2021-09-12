#include <QLineEdit>
#include <QCompleter>
#include <QRegExpValidator>
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

    connect(ui->devManufacturerNameInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devModelIdInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devVendorInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devProductInput, &TextLineEdit::valueChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devSleeperCheckBox, &QCheckBox::stateChanged, this, &DDF_Editor::deviceChanged);
    connect(ui->devStatusComboBox, &QComboBox::currentTextChanged, this, &DDF_Editor::deviceChanged);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &DDF_Editor::tabChanged);

    // d->u16Validator = new QRegExpValidator(QRegExp("0x[0-9a-fA-F]{1,4}"), this);
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
    if (!ddf.isValid())
    {
        return;
    }

    d->state = StateLoad;
    d->ddf = ddf;

    DDF_SortItems(d->ddf);

    int idx = ddf.path.indexOf(QLatin1String("/devices"));

    QString path;

    if (idx > 0)
    {
        path = ddf.path.mid(idx);
    }
    else
    {
        path = tr("*Untitled file");
    }

    setWindowTitle(QString("%1 - DDF Editor").arg(path));

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

    d->state = StateEdit;

    deviceChanged();
}

void DDF_Editor::previewDDF(const DeviceDescription &ddf)
{
    ui->ddfJsonDoc->setPlainText(DDF_ToJsonPretty(ddf));
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
}

void DDF_Editor::subDeviceSelected(uint subDevice)
{
    if (d->ddf.subDevices.size() <= subDevice)
    {
        return;
    }

    d->curSubDevice = subDevice;
    const DeviceDescription::SubDevice &sub = d->ddf.subDevices[d->curSubDevice];

    ui->subDeviceTypeInput->setInputText(sub.type);
    ui->subDeviceUniqueIdInput->setInputText(sub.uniqueId.join(QLatin1Char('-')));

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
}

void DDF_Editor::addSubDevice(const QString &name)
{
    {
        const auto i = std::find_if(d->ddf.subDevices.cbegin(), d->ddf.subDevices.cend(), [&](const auto &sub)
        {
            return d->dd->constantToString(sub.type) == name;
        });

        if (i != d->ddf.subDevices.cend())
        {
            DBG_Printf(DBG_INFO, "%s already exists\n", qPrintable(name));
            return;
        }
    }

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

        d->ddf.subDevices.push_back(sub);
        ui->ddfTreeView->setDDF(d->ddf);

        d->curItem = 0;
        subDeviceSelected(d->ddf.subDevices.size() - 1);
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
}

void DDF_Editor::tabChanged()
{
    if (ui->tabWidget->currentWidget() == ui->tabPreview)
    {
        previewDDF(d->ddf);
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
    const QString type = ui->subDeviceTypeInput->text();
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
    }
}
