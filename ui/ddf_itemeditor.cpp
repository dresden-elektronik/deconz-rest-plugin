#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <vector>
#include "ddf_itemeditor.h"

ItemLineEdit::ItemLineEdit(const QVariantMap &ddfParam, const DDF_FunctionDescriptor::Parameter &param, QWidget *parent) :
    QLineEdit(parent)
{
    paramDescription = param;

    if (ddfParam.contains(param.key))
    {
        QVariant val = ddfParam.value(param.key); // ["0x0001","0x0002"] --> "0x0001,0x0002"
        if (val.type() == QVariant::List)
        {
            val = val.toStringList().join(QLatin1Char(','));
        }

        setText(val.toString());
        origValue = text();
    }

    switch (param.dataType)
    {
    case DataTypeUInt16: {
        if (param.isHexString)
        {
            setPlaceholderText(QString("0x%1").arg(param.defaultValue, 4, 16, QLatin1Char('0')));
        }
        else
        {
            setPlaceholderText(QString::number(param.defaultValue));
        }

    } break;

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
    QWidget *widget = nullptr;
    QComboBox *functionComboBox = nullptr;
    QWidget *paramWidget = nullptr;
    QVariantMap paramMap;
    std::vector<QWidget*> itemWidgets; // dynamic widgets
    std::vector<ItemLineEdit*> paramLineEdits;
    void (DDF_ItemEditor::*paramChanged)() = nullptr; // called when the line edit content changes
};

class DDF_ItemEditorPrivate
{
public:
    DeviceDescriptions *dd = nullptr;
    QLabel *itemHeader = nullptr;
    QPlainTextEdit *itemDescription = nullptr;
    QScrollArea *scrollArea = nullptr;
    QCheckBox *publicCheckBox = nullptr;
    QCheckBox *staticCheckBox = nullptr;
    QCheckBox *awakeCheckBox = nullptr;
    QLineEdit *defaultValue = nullptr; // also static value

    DDF_Function readFunction;
    DDF_Function parseFunction;

    DeviceDescription::Item editItem;
};

DDF_ItemEditor::DDF_ItemEditor(QWidget *parent) :
    QWidget(parent)
{
    d  = new DDF_ItemEditorPrivate;
    QVBoxLayout *mainLay = new QVBoxLayout;
    setLayout(mainLay);
    mainLay->setMargin(0);

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
    scrollLay->addWidget(d->defaultValue);

    // parse function
    {
        DDF_Function &fn = d->parseFunction;
        fn.paramChanged = &DDF_ItemEditor::parseParamChanged;

        fn.widget = new QWidget(scrollWidget);
        scrollLay->addWidget(fn.widget);
        auto *fnLay = new QVBoxLayout;
        fn.widget->setLayout(fnLay);

        fnLay->addWidget(new QLabel(tr("Parse"), this));
        fn.functionComboBox = new QComboBox(fn.widget);
        fnLay->addWidget(fn.functionComboBox);

        fn.paramWidget = new QWidget(fn.widget);
        fnLay->addWidget(fn.paramWidget);
        fn.paramWidget->setLayout(new QFormLayout);
    }

    // read function
    {
        DDF_Function &fn = d->readFunction;
        fn.paramChanged = &DDF_ItemEditor::readParamChanged;

        fn.widget = new QWidget(scrollWidget);
        scrollLay->addWidget(fn.widget);
        auto *fnLay = new QVBoxLayout;
        fn.widget->setLayout(fnLay);

        fnLay->addWidget(new QLabel(tr("Read"), this));
        fn.functionComboBox = new QComboBox(fn.widget);
        fnLay->addWidget(fn.functionComboBox);

        fn.paramWidget = new QWidget(fn.widget);
        fnLay->addWidget(fn.paramWidget);
        fn.paramWidget->setLayout(new QFormLayout);
    }

    scrollLay->addStretch();

    // bottom bar
    QHBoxLayout *bottomLay = new QHBoxLayout;
    QPushButton *removeItemButton = new QPushButton(tr("Remove item"), this);
    removeItemButton->setToolTip(tr("Removes the item from the subdevice"));
    connect(removeItemButton, &QPushButton::clicked, this, &DDF_ItemEditor::removeItem);

    bottomLay->addStretch();
    bottomLay->addWidget(removeItemButton);

    mainLay->addLayout(bottomLay);
}

DDF_ItemEditor::~DDF_ItemEditor()
{
    delete d;
}

void DDF_ItemEditor::DDF_SetupFunction(DDF_Function &fn, const DeviceDescription::Item &item, const QVariantMap &ddfParam, const std::vector<DDF_FunctionDescriptor> &fnDescriptors)
{
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
    fn.functionComboBox->addItem(QObject::tr("None"));
    fn.paramMap = ddfParam;
    fn.paramLineEdits.clear();

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
            fnName = QLatin1String("zcl"); // default parse function
        }

        fn.functionComboBox->setCurrentText(fnName);
    }

    for (auto &descr : fnDescriptors)
    {
        if (descr.name == fn.functionComboBox->currentText())
        {
            QFormLayout *lay = static_cast<QFormLayout*>(fn.paramWidget->layout());

            for (const auto &param : descr.parameters)
            {
                QLabel *name = new QLabel(param.name, fn.paramWidget);
                fn.itemWidgets.push_back(name);

                ItemLineEdit *edit = new ItemLineEdit(ddfParam, param, fn.paramWidget);
                edit->setToolTip(param.description);
                fn.itemWidgets.push_back(edit);
                fn.paramLineEdits.push_back(edit);

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
        }
    }

    for (auto *edit : fn.paramLineEdits)
    {
        connect(edit, &ItemLineEdit::valueChanged, this, fn.paramChanged);
    }

    connect(fn.functionComboBox, &QComboBox::currentTextChanged, this, &DDF_ItemEditor::functionChanged);
}

void DDF_ItemEditor::setItem(const DeviceDescription::Item &item, DeviceDescriptions *dd)
{
    d->editItem = item;
    d->dd = dd;

    d->itemHeader->setText(QString("Item: %1").arg(QLatin1String(item.name.c_str())));
    d->publicCheckBox->setChecked(item.isPublic);
    d->awakeCheckBox->setChecked(item.awake);
    d->staticCheckBox->setChecked(item.isStatic);
    d->defaultValue->setText(item.defaultValue.toString());

    const auto &genItem = dd->getGenericItem(item.descriptor.suffix);

    d->itemDescription->setPlaceholderText(genItem.description);

    if (!genItem.description.isEmpty() && genItem.description == item.description)
    {
        d->itemDescription->setPlainText(QLatin1String(""));
    }
    else
    {
        d->itemDescription->setPlainText(item.description);
    }


    DDF_SetupFunction(d->parseFunction, item, item.parseParameters.toMap(), dd->getParseFunctions());

    DDF_SetupFunction(d->readFunction, item, item.readParameters.toMap(), dd->getReadFunctions());
}

const DeviceDescription::Item DDF_ItemEditor::item() const
{
    return d->editItem;
}

void DDF_ItemEditor::parseParamChanged()
{
    ItemLineEdit *edit = qobject_cast<ItemLineEdit*>(sender());
    Q_ASSERT(edit);
    Q_ASSERT(d->dd);

    DDF_Function &fn = d->parseFunction;

    edit->updateValueInMap(fn.paramMap);
    d->editItem.parseParameters = fn.paramMap;

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
    ItemLineEdit *edit = qobject_cast<ItemLineEdit*>(sender());
    Q_ASSERT(edit);
    Q_ASSERT(d->dd);

    DDF_Function &fn = d->readFunction;

    edit->updateValueInMap(fn.paramMap);
    d->editItem.readParameters = fn.paramMap;

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

void DDF_ItemEditor::attributeChanged()
{
    if (d->editItem.awake != d->awakeCheckBox->isChecked() ||
        d->editItem.isPublic != d->publicCheckBox->isChecked() ||
        d->editItem.isStatic != d->staticCheckBox->isChecked() ||
        d->editItem.description != d->itemDescription->toPlainText())
    {
        d->editItem.awake = d->awakeCheckBox->isChecked();
        d->editItem.isPublic = d->publicCheckBox->isChecked();
        d->editItem.isStatic = d->staticCheckBox->isChecked();
        d->editItem.description = d->itemDescription->toPlainText();

        if (d->editItem.isStatic)
        {
            d->parseFunction.widget->hide();
            d->readFunction.widget->hide();
        }
        else
        {
            d->parseFunction.widget->show();
            d->readFunction.widget->show();
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
            DDF_SetupFunction(d->parseFunction, item, item.parseParameters.toMap(), d->dd->getParseFunctions());
        }
        else if (d->readFunction.functionComboBox == combo)
        {
            DDF_SetupFunction(d->readFunction, item, item.readParameters.toMap(), d->dd->getReadFunctions());
        }
    }
}
