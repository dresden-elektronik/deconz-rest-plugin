#ifndef DDF_BINDING_EDITOR_H
#define DDF_BINDING_EDITOR_H

#include <QWidget>
#include <QFrame>
#include <vector>

class DDF_Binding;
class DDF_BindingEditorPrivate;
class DDF_ZclReport;
class QLabel;
class QLineEdit;
class QSpinBox;
class QModelIndex;

namespace deCONZ {
    class ZclCluster;
}

class DDF_ZclReportWidget : public QFrame
{
    Q_OBJECT

public:
    DDF_ZclReportWidget(QWidget *parent, DDF_ZclReport *rep, const deCONZ::ZclCluster *cl);

    const deCONZ::ZclCluster *cluster = nullptr;
    DDF_ZclReport *report = nullptr;
    QLineEdit *mfCode = nullptr;
    QLineEdit *attrId = nullptr;
    QLabel *attrName = nullptr;
    QLineEdit *dataType = nullptr;
    QSpinBox *minInterval = nullptr;
    QSpinBox *maxInterval = nullptr;
    QLineEdit *reportableChange = nullptr;

Q_SIGNALS:
    void changed();
    void removed();

public Q_SLOTS:
    void attributeIdChanged();
    void mfCodeChanged();
    void dataTypeChanged();
    void reportableChangeChanged();
    void minMaxChanged(int val);
};

class DDF_BindingEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DDF_BindingEditor(QWidget *parent = nullptr);
    ~DDF_BindingEditor();
    const std::vector<DDF_Binding> &bindings() const;
    void setBindings(const std::vector<DDF_Binding> &bindings);
    bool eventFilter(QObject *object, QEvent *event) override;

private Q_SLOTS:
    void bindingActivated(const QModelIndex &index, const QModelIndex &prev);
    void dropClusterUrl(const QUrl &url);
    void dropAttributeUrl(const QUrl &url);
    void reportRemoved();
    void removeBinding();

Q_SIGNALS:
    void bindingsChanged();

private:
    DDF_BindingEditorPrivate *d = nullptr;
};

#endif // DDF_BINDING_EDITOR_H
