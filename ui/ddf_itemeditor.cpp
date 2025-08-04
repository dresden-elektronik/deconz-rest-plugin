#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrlQuery>
#include <vector>
#include "deconz/zcl.h"
#include "ddf_itemeditor.h"

ItemLineEdit::ItemLineEdit(const QVariantMap &ddfParam, const DDF_FunctionDescriptor::Parameter &param, QWidget *parent) :
    QLineEdit(parent)
{
    setAcceptDrops(false);
    paramDescription = param;

    if (ddfParam.contains(param.key))
    {
        QVariant val = ddfParam.value(param.key); // ["0x0001","0x0002"] --> "0x0001,0x0002"
        if (val.type() == QVariant::List)
        {
            val = val.toStringList().join(QLatin1Char(','));
        }

        if (param.dataType == DataTypeUInt8 && param.key == QLatin1String("ep") && val.toUInt() == 0)
        {
            setText(QLatin1String("auto"));
        }
        else
        {
            setText(val.toString());
        }
        origValue = text();
    }

    switch (param.dataType)
    {
    case DataTypeUInt8:
    {
        if (param.isHexString)
        {
            setPlaceholderText(QString("0x%1").arg(param.defaultValue.toUInt(), 2, 16, QLatin1Char('0')));
        }
        else
        {
            setPlaceholderText(QString::number(param.defaultValue.toUInt()));
        }
    }
        break;

    case DataTypeUInt16:
    {
        if (param.isHexString)
        {
            setPlaceholderText(QString("0x%1").arg(param.defaultValue.toUInt(), 4, 16, QLatin1Char('0')));
        }
        else
        {
            setPlaceholderText(QString::number(param.defaultValue.toUInt()));
        }
    }
        break;

    case DataTypeString:
    {
        if (!param.defaultValue.isNull() && text().isEmpty())
        {
            setPlaceholderText(text());
        }
    }
        break;

    default:
        break;
    }

    verifyInputText(text());

    connect(this, &QLineEdit::textChanged, this, &ItemLineEdit::inputTextChanged);
}

void ItemLineEdit::inputTextChanged(const QString &text)
{
    if (verifyInputText(text))
    {
        emit valueChanged();
    }
}

bool ItemLineEdit::verifyInputText(const QString &text)
{
    bool isValid = true;

    const QStringList ls = text.split(QLatin1Char(','), SKIP_EMPTY_PARTS);

    for (const QString &i : ls)
    {
        if (paramDescription.dataType == DataTypeUInt8 && paramDescription.key == QLatin1String("ep"))
        {
            if (i == QLatin1String("auto"))
            {
                isValid = true;
                continue;
            }
        }

        if (i.isEmpty() && !paramDescription.isOptional)
        {
            isValid = false;
        }
        else if (!i.isEmpty())
        {
            switch (paramDescription.dataType)
            {
            case DataTypeUInt8:
            case DataTypeUInt16:
            case DataTypeUInt32:
            case DataTypeUInt64:
            {
                bool ok;
                qint64 num = i.toULongLong(&ok, 0);

                if (!ok) { isValid = false; }
                else if (paramDescription.dataType == DataTypeUInt8 && num > UINT8_MAX) { isValid = false;  }
                else if (paramDescription.dataType == DataTypeUInt16 && num > UINT16_MAX) { isValid = false;  }
                else if (paramDescription.dataType == DataTypeUInt32 && num > UINT32_MAX) { isValid = false;  }
            }
                break;

            default:
                break;
            }
        }
    }

    if (!isValid)
    {
        setStyleSheet(QLatin1String("color:red"));
    }
    else if (text != origValue)
    {
        setStyleSheet(QLatin1String("color:blue"));
    }
    else
    {
        setStyleSheet(QString());
    }

    return isValid;
}

void ItemLineEdit::updateValueInMap(QVariantMap &map) const
{
    QVariantList vls;
    const QStringList sls = text().split(QLatin1Char(','), SKIP_EMPTY_PARTS);

    int fieldWidth = 0;

    for (const QString &i : sls)
    {
        if (paramDescription.dataType == DataTypeUInt8 && paramDescription.key == QLatin1String("ep"))
        {
            if (i == QLatin1String("auto"))
            {
                vls.push_back(0);
                continue;
            }
        }

        switch (paramDescription.dataType)
        {
        case DataTypeUInt8:  fieldWidth = 2;  break;
        case DataTypeUInt16: fieldWidth = 4;  break;
        case DataTypeUInt32: fieldWidth = 8;  break;
        case DataTypeUInt64: fieldWidth = 16; break;

        case DataTypeString:
        {
            vls.push_back(i);
        }
            break;

        default:
            break;
        }

        if (fieldWidth > 0)
        {
            bool ok;
            qint64 num = i.toULongLong(&ok, 0);

            if (!ok)
            {  }
            else if (paramDescription.isHexString)
            {
                vls.push_back(QString("0x%1").arg(num, fieldWidth, 16, QLatin1Char('0')));
            }
            else
            {
                vls.push_back(num);
            }
        }
    }

    if (vls.size() == 1)
    {
        map[paramDescription.key] = vls.front();
    }
    else if (vls.size() > 1)
    {
        map[paramDescription.key] = vls;
    }
    else
    {
        map[paramDescription.key] = QVariant();
    }
}

struct DDF_Function
{
    FunctionWidget *widget = nullptr;
    QComboBox *functionComboBox = nullptr;
    QWidget *paramWidget = nullptr;
    QVariantMap paramMap;
    QLabel *clusterName = nullptr;
    QLabel *attrName = nullptr;
    std::vector<QWidget*> itemWidgets; // dynamic widgets
    void (DDF_ItemEditor::*paramChanged)() = nullptr; // called when the line edit content changes
};

enum EditorState
{
    StateInit,
    StateLoad,
    StateEdit
};

class DDF_ItemEditorPrivate
{
public:
    EditorState state = StateInit;
    DeviceDescriptions *dd = nullptr;
    QLabel *itemHeader = nullptr;
    QPlainTextEdit *itemDescription = nullptr;
    QScrollArea *scrollArea = nullptr;
    QCheckBox *publicCheckBox = nullptr;
    QCheckBox *staticCheckBox = nullptr;
    QCheckBox *awakeCheckBox = nullptr;
    QLineEdit *defaultValue = nullptr; // also static value

    QSpinBox *readInterval = nullptr;
    DDF_Function readFunction;
    DDF_Function parseFunction;
    DDF_Function writeFunction;

    DeviceDescription::Item editItem;
};

DDF_ItemEditor::DDF_ItemEditor(QWidget *parent) :
    QWidget(parent)
{
    d  = new DDF_ItemEditorPrivate;
    QVBoxLayout *mainLay = new QVBoxLayout;
    setLayout(mainLay);
    mainLay->setContentsMargins(0,0,0,0);

    setAcceptDrops(true);

    d->itemHeader = new QLabel(tr("Item"), this);
    mainLay->addWidget(d->itemHeader);

    d->scrollArea = new QScrollArea(this);

    QWidget *scrollWidget = new QWidget;
    d->scrollArea->setWidget(scrollWidget);
    d->scrollArea->setWidgetResizable(true);

    QVBoxLayout *scrollLay = new QVBoxLayout;
    scrollWidget->setLayout(scrollLay);

    scrollWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    mainLay->addWidget(d->scrollArea);

    QLabel *description = new QLabel(tr("Description"), scrollWidget);
    scrollLay->addWidget(description);

    d->itemDescription = new QPlainTextEdit(scrollWidget);
    d->itemDescription->setAcceptDrops(false);
    d->itemDescription->setMinimumHeight(32);
    d->itemDescription->setMaximumHeight(72);
    d->itemDescription->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    connect(d->itemDescription, &QPlainTextEdit::textChanged, this, &DDF_ItemEditor::attributeChanged);
    scrollLay->addWidget(d->itemDescription);

    d->publicCheckBox = new QCheckBox(tr("Public item"));
    d->publicCheckBox->setToolTip(tr("The item is visible in the REST-API"));
    scrollLay->addWidget(d->publicCheckBox);
    connect(d->publicCheckBox, &QCheckBox::stateChanged, this, &DDF_ItemEditor::attributeChanged);


    d->awakeCheckBox = new QCheckBox(tr("Awake on receive"));
    d->awakeCheckBox->setToolTip(tr("The device is considered awake when this item is set due a incoming command."));
    scrollLay->addWidget(d->awakeCheckBox);
    connect(d->awakeCheckBox, &QCheckBox::stateChanged, this, &DDF_ItemEditor::attributeChanged);

    d->staticCheckBox = new QCheckBox(tr("Static default value"));
    d->staticCheckBox->setToolTip(tr("A static default value is fixed and can't be changed."));
    scrollLay->addWidget(d->staticCheckBox);
    connect(d->staticCheckBox, &QCheckBox::stateChanged, this, &DDF_ItemEditor::attributeChanged);

    scrollLay->addWidget(new QLabel(tr("Default value")));

    d->defaultValue = new QLineEdit;
    d->defaultValue->setAcceptDrops(false);
    connect(d->defaultValue, &QLineEdit::textChanged, this, &DDF_ItemEditor::attributeChanged);
    scrollLay->addWidget(d->defaultValue);

    QFont boldFont = font();
    boldFont.setBold(true);

    // parse function
    {
        DDF_Function &fn = d->parseFunction;
        fn.paramChanged = &DDF_ItemEditor::parseParamChanged;

        fn.widget = new FunctionWidget(scrollWidget);
        scrollLay->addWidget(fn.widget);
        auto *fnLay = new QVBoxLayout;
        fn.widget->setLayout(fnLay);
        connect(fn.widget, &FunctionWidget::droppedUrl, this, &DDF_ItemEditor::droppedUrl);

        QLabel *parseLabel = new QLabel(tr("Parse"), this);
        parseLabel->setFont(boldFont);
        fnLay->addWidget(parseLabel);
        fn.functionComboBox = new QComboBox(fn.widget);
        fn.functionComboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        fn.functionComboBox->setMinimumWidth(160);
        fnLay->addWidget(fn.functionComboBox);

        fn.paramWidget = new QWidget(fn.widget);
        fnLay->addWidget(fn.paramWidget);
        fn.paramWidget->setLayout(new QFormLayout);
    }

    // read function
    {
        DDF_Function &fn = d->readFunction;
        fn.paramChanged = &DDF_ItemEditor::readParamChanged;

        fn.widget = new FunctionWidget(scrollWidget);
        scrollLay->addWidget(fn.widget);
        auto *fnLay = new QVBoxLayout;
        fn.widget->setLayout(fnLay);
        connect(fn.widget, &FunctionWidget::droppedUrl, this, &DDF_ItemEditor::droppedUrl);

        QLabel *readLabel = new QLabel(tr("Read"), this);
        readLabel->setFont(boldFont);
        fnLay->addWidget(readLabel);
        fn.functionComboBox = new QComboBox(fn.widget);
        fn.functionComboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        fn.functionComboBox->setMinimumWidth(160);
        fnLay->addWidget(fn.functionComboBox);

        fn.paramWidget = new QWidget(fn.widget);
        fnLay->addWidget(fn.paramWidget);
        QFormLayout *readLay = new QFormLayout;
        fn.paramWidget->setLayout(readLay);

        d->readInterval = new QSpinBox(this);
        d->readInterval->setSuffix(" s");
        d->readInterval->setRange(0, 84000 * 2);
        connect(d->readInterval, SIGNAL(valueChanged(int)), this, SLOT(attributeChanged()));
        readLay->addRow(new QLabel(tr("Interval")), d->readInterval);
    }

    // write function
    {
        DDF_Function &fn = d->writeFunction;
        fn.paramChanged = &DDF_ItemEditor::writeParamChanged;

        fn.widget = new FunctionWidget(scrollWidget);
        scrollLay->addWidget(fn.widget);
        auto *fnLay = new QVBoxLayout;
        fn.widget->setLayout(fnLay);
        connect(fn.widget, &FunctionWidget::droppedUrl, this, &DDF_ItemEditor::droppedUrl);

        QLabel *writeLabel = new QLabel(tr("Write"), this);
        writeLabel->setFont(boldFont);
        fnLay->addWidget(writeLabel);
        fn.functionComboBox = new QComboBox(fn.widget);
        fn.functionComboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        fn.functionComboBox->setMinimumWidth(160);
        fnLay->addWidget(fn.functionComboBox);

        fn.paramWidget = new QWidget(fn.widget);
        fnLay->addWidget(fn.paramWidget);
        fn.paramWidget->setLayout(new QFormLayout);
    }

    scrollLay->addStretch();
}

DDF_ItemEditor::~DDF_ItemEditor()
{
    delete d;
}

void DDF_ItemEditor::updateZclLabels(DDF_Function &fn)
{
    bool ok;
    quint16 clusterId = UINT16_MAX;
    quint16 attrId = UINT16_MAX;

    if (fn.paramMap.contains(QLatin1String("cl")))
    {
        clusterId = fn.paramMap.value(QLatin1String("cl")).toString().toUInt(&ok, 0);
    }

    if (clusterId == UINT16_MAX)
    {
        return;
    }

    const auto cl = deCONZ::ZCL_InCluster(HA_PROFILE_ID, clusterId, 0x0000);

    if (!cl.isValid())
    {
        return;
    }

    if (fn.clusterName && clusterId != UINT16_MAX)
    {
        fn.clusterName->setText(cl.name());
    }

    if (fn.paramMap.contains(QLatin1String("at")))
    {
        attrId = fn.paramMap.value(QLatin1String("at")).toString().toUInt(&ok, 0);
    }

    if (fn.attrName && attrId != UINT16_MAX)
    {
        const auto i = std::find_if(cl.attributes().cbegin(), cl.attributes().cend(), [attrId](const auto &i)
        {
            return i.id() == attrId;
        });

        if (i != cl.attributes().cend())
        {
            fn.attrName->setText(i->name());
        }
    }
}

void DDF_ItemEditor::setupFunction(DDF_Function &fn, const DeviceDescription::Item &item, const QVariantMap &ddfParam, const std::vector<DDF_FunctionDescriptor> &fnDescriptors)
{
    fn.paramWidget->hide();
    fn.attrName = nullptr;
    fn.clusterName = nullptr;

    for (auto *w : fn.itemWidgets)
    {
        w->hide();
        w->deleteLater();
    }

    disconnect(fn.functionComboBox, &QComboBox::currentTextChanged, this, &DDF_ItemEditor::functionChanged);

    fn.itemWidgets.clear();

    if (item.isStatic) { fn.widget->hide(); }
    else               { fn.widget->show(); }

    fn.functionComboBox->clear();
    fn.functionComboBox->setToolTip(QString());
    fn.functionComboBox->addItem(QObject::tr("None"));
    fn.paramMap = ddfParam;

    for (auto &descr : fnDescriptors)
    {
        fn.functionComboBox->addItem(descr.name);
    }

    QString fnName;

    if (!ddfParam.isEmpty())
    {
        if (ddfParam.contains(QLatin1String("fn")))
        {
            fnName = ddfParam.value(QLatin1String("fn")).toString();
        }
        else
        {
            fnName = QLatin1String("zcl:attr"); // default parse function
        }

        fn.functionComboBox->setCurrentText(fnName);
    }

    for (auto &descr : fnDescriptors)
    {
        if (descr.name == fn.functionComboBox->currentText())
        {
            fn.functionComboBox->setToolTip(descr.description);
            QFormLayout *lay = static_cast<QFormLayout*>(fn.paramWidget->layout());

            for (const auto &param : descr.parameters)
            {
                if (fnName == QLatin1String("zcl") || fnName == QLatin1String("zcl:attr"))
                {
                    if (param.key == QLatin1String("cl"))
                    {
                        auto *label = new QLabel("Cluster");
                        fn.clusterName = new QLabel;
                        fn.clusterName->setWordWrap(true);
                        fn.itemWidgets.push_back(label);
                        fn.itemWidgets.push_back(fn.clusterName);
                        lay->insertRow(0, label, fn.clusterName);
                    }
                    else if (param.key == QLatin1String("at"))
                    {
                        auto *label = new QLabel("Attribute");
                        fn.attrName = new QLabel;
                        fn.attrName->setWordWrap(true);
                        fn.itemWidgets.push_back(label);
                        fn.itemWidgets.push_back(fn.attrName);
                        lay->insertRow(1, label, fn.attrName);
                    }
                }

                QLabel *name = new QLabel(param.name, fn.paramWidget);
                fn.itemWidgets.push_back(name);

                ItemLineEdit *edit = new ItemLineEdit(ddfParam, param, fn.paramWidget);
                edit->setToolTip(param.description);
                fn.itemWidgets.push_back(edit);
                connect(edit, &ItemLineEdit::valueChanged, this, fn.paramChanged);

                if (param.dataType == DataTypeString)
                {
                    lay->addRow(name);
                    lay->addRow(edit);
                }
                else
                {
                    lay->addRow(name, edit);
                }
            }

            break;
        }
    }

    if (fn.functionComboBox->currentIndex() != 0) // none
    {
        fn.paramWidget->show();
    }

    connect(fn.functionComboBox, &QComboBox::currentTextChanged, this, &DDF_ItemEditor::functionChanged);
    updateZclLabels(fn);
}

void DDF_ItemEditor::setItem(const DeviceDescription::Item &item, DeviceDescriptions *dd)
{
    d->state = StateInit;
    d->editItem = item;
    d->dd = dd;

    d->itemHeader->setText(QString("%1  (%2)")
                           .arg(QLatin1String(item.name.c_str()))
                           .arg(R_DataTypeToString(item.descriptor.type)));
    d->publicCheckBox->setChecked(item.isPublic);
    d->awakeCheckBox->setChecked(item.awake);
    d->staticCheckBox->setChecked(item.isStatic);
    d->defaultValue->setText(item.defaultValue.toString());
    if (item.refreshInterval >= 0)
    {
        d->readInterval->setValue(item.refreshInterval);
    }
    else
    {
        d->readInterval->setValue(0);
    }

    const auto &genItem = dd->getGenericItem(item.descriptor.suffix);

    d->itemDescription->setPlaceholderText(genItem.description);

    if (!genItem.description.isEmpty() && genItem.description == item.description)
    {
        d->itemDescription->setPlainText(QLatin1String(""));
        d->editItem.description.clear();
    }
    else
    {
        d->itemDescription->setPlainText(item.description);
    }

    setupFunction(d->parseFunction, item, item.parseParameters.toMap(), dd->getParseFunctions());
    setupFunction(d->readFunction, item, item.readParameters.toMap(), dd->getReadFunctions());
    setupFunction(d->writeFunction, item, item.writeParameters.toMap(), dd->getWriteFunctions());
    d->state = StateEdit;

    if (item != d->editItem)
    {
        emit itemChanged();
    }
}

const DeviceDescription::Item DDF_ItemEditor::item() const
{
    return d->editItem;
}

void DDF_ItemEditor::parseParamChanged()
{
    Q_ASSERT(d->dd);

    DDF_Function &fn = d->parseFunction;
    ItemLineEdit *edit = qobject_cast<ItemLineEdit*>(sender());

    if (edit)
    {
        edit->updateValueInMap(fn.paramMap);
    }

    if (d->editItem.parseParameters != fn.paramMap)
    {
        d->editItem.parseParameters = fn.paramMap;
        updateZclLabels(fn);
    }

    const auto &genItem = d->dd->getGenericItem(d->editItem.descriptor.suffix);

    if (genItem.parseParameters == d->editItem.parseParameters)
    {
        d->editItem.isGenericParse = 1;
        d->editItem.isImplicit = genItem.isImplicit; // todo is implicit shouldn't be used here
    }
    else
    {
        d->editItem.isGenericParse = 0;
        d->editItem.isImplicit = 0;
    }

    emit itemChanged();
}

void DDF_ItemEditor::readParamChanged()
{
    Q_ASSERT(d->dd);

    DDF_Function &fn = d->readFunction;
    ItemLineEdit *edit = qobject_cast<ItemLineEdit*>(sender());

    if (edit)
    {
        edit->updateValueInMap(fn.paramMap);
    }

    if (d->editItem.readParameters != fn.paramMap)
    {
        d->editItem.readParameters = fn.paramMap;
        updateZclLabels(fn);
    }

    const auto &genItem = d->dd->getGenericItem(d->editItem.descriptor.suffix);

    if (genItem.readParameters == d->editItem.readParameters)
    {
        d->editItem.isGenericRead = 1;
        d->editItem.isImplicit = genItem.isImplicit; // todo is implicit shouldn't be used here
    }
    else
    {
        d->editItem.isGenericRead = 0;
        d->editItem.isImplicit = 0;
    }

    emit itemChanged();
}

void DDF_ItemEditor::writeParamChanged()
{
    Q_ASSERT(d->dd);

    DDF_Function &fn = d->writeFunction;
    ItemLineEdit *edit = qobject_cast<ItemLineEdit*>(sender());

    if (edit)
    {
        edit->updateValueInMap(fn.paramMap);
    }

    if (d->editItem.writeParameters != fn.paramMap)
    {
        d->editItem.writeParameters = fn.paramMap;
        updateZclLabels(fn);
    }

    const auto &genItem = d->dd->getGenericItem(d->editItem.descriptor.suffix);

    if (genItem.writeParameters == d->editItem.writeParameters)
    {
        d->editItem.isGenericWrite = 1;
        d->editItem.isImplicit = genItem.isImplicit; // todo is implicit shouldn't be used here
    }
    else
    {
        d->editItem.isGenericWrite = 0;
        d->editItem.isImplicit = 0;
    }

    emit itemChanged();
}

void DDF_ItemEditor::attributeChanged()
{
    if (d->state != StateEdit)
    {
        return;
    }

    if (d->editItem.awake != d->awakeCheckBox->isChecked() ||
        d->editItem.isPublic != d->publicCheckBox->isChecked() ||
        d->editItem.isStatic != d->staticCheckBox->isChecked() ||
        d->editItem.refreshInterval != d->readInterval->value() ||
        d->editItem.description != d->itemDescription->toPlainText() ||
        d->editItem.defaultValue.toString() != d->defaultValue->text())
    {
        d->editItem.awake = d->awakeCheckBox->isChecked();
        d->editItem.isPublic = d->publicCheckBox->isChecked();
        d->editItem.isStatic = d->staticCheckBox->isChecked();
        d->editItem.description = d->itemDescription->toPlainText();
        d->editItem.refreshInterval = d->readInterval->value();
        if (d->editItem.refreshInterval <= 0)
        {
            d->editItem.refreshInterval = DeviceDescription::Item::NoRefreshInterval;
        }

        if (!d->defaultValue->text().isEmpty())
        {
            switch (d->editItem.descriptor.qVariantType)
            {
            case QVariant::Double:
            {
                bool ok;
                double val = d->defaultValue->text().toDouble(&ok);
                if (ok)
                {
                    d->editItem.defaultValue = val;
                }
            }
                break;

            case QVariant::String:
            {
                d->editItem.defaultValue = d->defaultValue->text();
            }
                break;

            case QVariant::Bool:
            {
                if (d->defaultValue->text() == QLatin1String("true") || d->defaultValue->text() == QLatin1String("1"))
                {
                    d->editItem.defaultValue = true;
                }
                else if (d->defaultValue->text() == QLatin1String("false") || d->defaultValue->text() == QLatin1String("0"))
                {
                    d->editItem.defaultValue = false;
                }
                else
                {
                    d->editItem.defaultValue = {};
                }
            }
                break;

            default:
                break;
            }
        }
        else
        {
            d->editItem.defaultValue = {};
        }

        if (d->editItem.isStatic)
        {
            d->parseFunction.widget->hide();
            d->readFunction.widget->hide();
            d->writeFunction.widget->hide();
        }
        else
        {
            d->parseFunction.widget->show();
            d->readFunction.widget->show();
            d->writeFunction.widget->show();
        }

        emit itemChanged();
    }
}

void DDF_ItemEditor::functionChanged(const QString &text)
{
    DDF_Function *fn = nullptr;
    QVariant *fnParam = nullptr;
    QString prevFunction;
    QComboBox *combo = qobject_cast<QComboBox*>(sender());

    if (!combo)
    {
        return;
    }

    if (d->parseFunction.functionComboBox == combo)
    {
        fn = &d->parseFunction;
        fnParam = &d->editItem.parseParameters;
    }
    else if (d->readFunction.functionComboBox == combo)
    {
        fn = &d->readFunction;
        fnParam = &d->editItem.readParameters;
    }
    else if (d->writeFunction.functionComboBox == combo)
    {
        fn = &d->writeFunction;
        fnParam = &d->editItem.writeParameters;
    }
    else
    {
        return;
    }

    if (fn->paramMap.contains("fn"))
    {
        prevFunction = fn->paramMap.value("fn").toString();
    }

    if (prevFunction != text)
    {
        if (!prevFunction.isEmpty())
        {
            auto btn = QMessageBox::question(parentWidget(),
                                             tr("Change function to %1").arg(text),
                                             tr("Proceed? Current function settings will be lost."));
            if (btn == QMessageBox::No)
            {
                fn->functionComboBox->setCurrentText(prevFunction);
                return;
            }
        }

        fn->paramMap = {};
        fn->paramMap["fn"] = text;
        *fnParam = fn->paramMap;

        auto &item = d->editItem;

        if (d->parseFunction.functionComboBox == combo)
        {
            setupFunction(d->parseFunction, item, item.parseParameters.toMap(), d->dd->getParseFunctions());
        }
        else if (d->readFunction.functionComboBox == combo)
        {
            setupFunction(d->readFunction, item, item.readParameters.toMap(), d->dd->getReadFunctions());
        }
        else if (d->writeFunction.functionComboBox == combo)
        {
            setupFunction(d->writeFunction, item, item.writeParameters.toMap(), d->dd->getWriteFunctions());
        }
    }
}

void DDF_ItemEditor::droppedUrl(const QUrl &url)
{
    if (url.scheme() == QLatin1String("zclattr"))
    {
        // zclattr:attr?ep=1&cl=6&cs=s&mf=0&a=0&dt=16&rmin=1&rmax=300&t=D
        QUrlQuery urlQuery(url);

        bool ok;
        QVariantMap params;

        if (sender() == d->parseFunction.widget)
        {
            params = d->editItem.parseParameters.toMap();
        }
        else if (sender() == d->readFunction.widget)
        {
            params = d->editItem.readParameters.toMap();
        }
        else if (sender() == d->writeFunction.widget)
        {
            params = d->editItem.writeParameters.toMap();
        }

        if (urlQuery.hasQueryItem("ep"))
        {
            params["ep"] = urlQuery.queryItemValue("ep").toUInt(&ok, 16);
        }
        if (urlQuery.hasQueryItem("cid"))
        {
            quint16 cl = urlQuery.queryItemValue("cid").toUShort(&ok, 16);
            params["cl"] = QString("0x%1").arg(cl, 4, 16, QLatin1Char('0'));
        }
        if (urlQuery.hasQueryItem("a"))
        {
            quint16 attr = urlQuery.queryItemValue("a").toUShort(&ok, 16);
            params["at"] = QString("0x%1").arg(attr, 4, 16, QLatin1Char('0'));
        }
        if (urlQuery.hasQueryItem("mf"))
        {
            quint16 mf = urlQuery.queryItemValue("mf").toUShort(&ok, 16);
            if (mf != 0)
            {
                params["mf"] = QString("0x%1").arg(mf, 4, 16, QLatin1Char('0'));
            }
            else
            {
                params.remove("mf");
            }
        }

        if (sender() == d->parseFunction.widget)
        {
            //d->editItem.parseParameters = params;
            setupFunction(d->parseFunction, d->editItem, params, d->dd->getParseFunctions());
            parseParamChanged();
        }
        else if (sender() == d->readFunction.widget)
        {
            if (urlQuery.hasQueryItem("rmax"))
            {
                int max = urlQuery.queryItemValue("rmax").toInt();
                d->readInterval->setValue(max);
            }
            //d->editItem.readParameters = params;
            setupFunction(d->readFunction, d->editItem, params, d->dd->getReadFunctions());
            readParamChanged();
        }
        else if (sender() == d->writeFunction.widget)
        {
            //d->editItem.writeParameters = params;
            setupFunction(d->writeFunction, d->editItem, params, d->dd->getWriteFunctions());
            writeParamChanged();
        }

        //emit itemChanged();
    }
}

FunctionWidget::FunctionWidget(QWidget *parent) :
    QWidget(parent)
{
    setAcceptDrops(true);
}

void FunctionWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
    {
        return;
    }

    window()->raise();

    const auto urls = event->mimeData()->urls();
    const auto url = urls.first();

    if (url.scheme() == QLatin1String("zclattr"))
    {
        event->accept();
        QPalette pal = parentWidget()->palette();
        pal.setColor(QPalette::Window, pal.color(QPalette::AlternateBase));
        setPalette(pal);
        setAutoFillBackground(true);
    }
}

void FunctionWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
    setPalette(parentWidget()->palette());
}

void FunctionWidget::dropEvent(QDropEvent *event)
{
    setPalette(parentWidget()->palette());

    if (!event->mimeData()->hasUrls())
    {
        return;
    }

    const auto urls = event->mimeData()->urls();
    const auto url = urls.first();
    emit droppedUrl(url);
}
