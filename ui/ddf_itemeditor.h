#ifndef DDF_ITEMEDITOR_H
#define DDF_ITEMEDITOR_H

#include <QWidget>
#include <QLineEdit>
#include "device_descriptions.h"

class ItemLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    ItemLineEdit(const QVariantMap &ddfParam, const DDF_FunctionDescriptor::Parameter &param, QWidget *parent);

    void updateValueInMap(QVariantMap &map) const;

private Q_SLOTS:
    void inputTextChanged(const QString &text);
    bool verifyInputText(const QString &text);

Q_SIGNALS:
    void valueChanged();

private:
    QString origValue;
    DDF_FunctionDescriptor::Parameter paramDescription;
};

class DDF_ItemEditorPrivate;
struct DDF_Function;

class DDF_ItemEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DDF_ItemEditor(QWidget *parent = nullptr);
    ~DDF_ItemEditor();

    void setItem(const DeviceDescription::Item &item, DeviceDescriptions *dd);
    const DeviceDescription::Item item() const;

public Q_SLOTS:
    void parseParamChanged();
    void readParamChanged();
    void attributeChanged();
    void functionChanged(const QString &text);

Q_SIGNALS:
    void removeItem();
    void itemChanged();

private:
    void DDF_SetupFunction(DDF_Function &fn, const DeviceDescription::Item &item, const QVariantMap &ddfParam, const std::vector<DDF_FunctionDescriptor> &fnDescriptors);
    DDF_ItemEditorPrivate *d = nullptr;
};

#endif // DDF_ITEMEDITOR_H
