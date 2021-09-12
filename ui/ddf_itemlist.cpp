#include <QMimeData>
#include <QStandardItemModel>
#include <QAbstractListModel>
#include <QUrl>
#include "deconz/dbg_trace.h"
#include "device_descriptions.h"
#include "ddf_itemlist.h"

const char *ddfMimeItemName = "ddf/itemname";

#define ITEM_TYPE_ROLE (Qt::UserRole + 2)
#define ITEM_TYPE_SUBDEVICE 1
#define ITEM_TYPE_DDF_ITEM  2

class ItemModel : public QStandardItemModel
{
public:
    ItemModel(QObject *parent) :
        QStandardItemModel(parent)
    {

    }

    QStringList mimeTypes() const override
    {
        return {"text/uri-list"};
    }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        QMimeData *mime = new QMimeData;

        QList<QUrl> urls;

        DBG_Printf(DBG_INFO, "mime data, indexes.size %d\n", indexes.size());

        for (const QModelIndex &idx : indexes)
        {
            QUrl url;
            int role = idx.data(ITEM_TYPE_ROLE).toInt();
            if (role == ITEM_TYPE_DDF_ITEM)
            {
                url.setScheme("ddfitem");
            }
            else if (role == ITEM_TYPE_SUBDEVICE)
            {
                url.setScheme("subdevice");
            }
            else
            {
                continue; // TODO
            }
            url.setPath(idx.data().toString());
            urls.push_back(url);
        }
        mime->setUrls(urls);
        return mime;
    }

private:

};

class DDF_ItemListPrivate
{
public:
    ItemModel *model;
};

DDF_ItemList::DDF_ItemList(QWidget *parent) :
    QListView(parent)
{
    d = new DDF_ItemListPrivate;
    d->model = new ItemModel(this);
    setModel(d->model);
    setDragDropMode(QListView::DragOnly);
}

DDF_ItemList::~DDF_ItemList()
{
    delete d;
}

void DDF_ItemList::update(DeviceDescriptions *dd)
{
    d->model->clear();

    {
        for (const auto &sub : dd->getSubDevices())
        {
            QStandardItem *item = new QStandardItem(sub.name);
            //item->setToolTip(i.description);
            item->setData(ITEM_TYPE_SUBDEVICE, ITEM_TYPE_ROLE);
            d->model->appendRow(item);
        }
    }

    {
        auto gen = dd->genericItems();

        std::sort(gen.begin(), gen.end(), [](const auto &a, const auto &b){ return a.name < b.name; });

        for (const auto &i : gen)
        {
            QStandardItem *item = new QStandardItem(i.name.c_str());
            item->setToolTip(i.description);
            item->setData(ITEM_TYPE_DDF_ITEM, ITEM_TYPE_ROLE);
            d->model->appendRow(item);
        }
    }
}
