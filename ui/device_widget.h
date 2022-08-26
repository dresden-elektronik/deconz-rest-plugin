#ifndef DEVICE_WIDGET_H
#define DEVICE_WIDGET_H

#include <QWidget>
#include "device.h"

namespace Ui {
class DeviceWidget;
}

namespace deCONZ {
    class NodeEvent;
}

class DeviceDescription;
class DeviceWidgetPrivat;
class Event;

class DeviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceWidget(DeviceContainer &devices, QWidget *parent = nullptr);
    ~DeviceWidget();

    void handleEvent(const Event &event);
    void nodeEvent(const deCONZ::NodeEvent &event);

    void displayDDF(const DeviceDescription &ddf);

Q_SIGNALS:
    void permitJoin(int seconds);

public Q_SLOTS:
    void editDDF();
    void openDDF();
    void saveDDF();
    void saveAsDDF();
    void hotReload();

private Q_SLOTS:
    void enablePermitJoin();
    void disablePermitJoin();
    void enableDDFHandlingChanged();
    void reloadTimerFired();

private:
    Ui::DeviceWidget *ui;
    DeviceWidgetPrivat *d = nullptr;
};

#endif // DEVICE_WIDGET_H
