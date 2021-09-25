#ifndef DDF_EDITOR_H
#define DDF_EDITOR_H

#include <QWidget>

namespace Ui {
class DDF_Editor;
}

class DeviceDescriptions;
class DeviceDescription;

class DDF_EditorPrivate;

class DDF_Editor : public QWidget
{
    Q_OBJECT

public:
    explicit DDF_Editor(DeviceDescriptions *dd, QWidget *parent = nullptr);
    ~DDF_Editor();

    void setDDF(const DeviceDescription &ddf);
    void previewDDF(const DeviceDescription &ddf);
    void updateDDFHash();

    const DeviceDescription &ddf() const;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private Q_SLOTS:
    void itemSelected(uint subDevice, uint item);
    void itemChanged();
    void subDeviceSelected(uint subDevice);
    void deviceSelected();
    void addItem(uint subDevice, const QString &suffix);
    void addSubDevice(const QString &name);
    void deviceChanged();
    void tabChanged();
    void removeItem(uint subDevice, uint item);
    void removeSubDevice(uint subDevice);
    void subDeviceInputChanged();
    void bindingsChanged();
    void startCheckDDFChanged();
    void checkDDFChanged();

private:
    Ui::DDF_Editor *ui;
    DDF_EditorPrivate *d = nullptr;
};

#endif // DDF_EDITOR_H
