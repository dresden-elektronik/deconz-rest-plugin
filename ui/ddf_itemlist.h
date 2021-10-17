#ifndef DDF_ITEMLIST_H
#define DDF_ITEMLIST_H

#include <QListView>

class DDF_ItemListPrivate;
class DeviceDescriptions;

class DDF_ItemList : public QListView
{
public:
    DDF_ItemList(QWidget *parent = nullptr);
    ~DDF_ItemList();

    void update(DeviceDescriptions *dd);

private:
    DDF_ItemListPrivate *d = nullptr;
};

#endif // DDF_ITEMLIST_H
