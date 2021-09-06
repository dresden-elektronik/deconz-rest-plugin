#include <QMimeData>
#include <QStandardItemModel>
#include <QAbstractListModel>
#include <QUrl>
#include "deconz/dbg_trace.h"
#include "device_descriptions.h"
#include "ddf_itemlist.h"

const char *ddfMimeItemName = "ddf/itemname";

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
            url.setScheme("ddfitem");
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

    const DDF_Items &gen = dd->genericItems();

    std::vector<BufString<64>> names;
    names.reserve(gen.size());

    for (const auto &i : gen)
    {
        Q_ASSERT(!i.name.empty());
        names.push_back(i.name);
    }

    std::sort(names.begin(), names.end());

    for (const auto &name : names)
    {
        QStandardItem *item = new QStandardItem(name.c_str());
        d->model->appendRow(item);
    }
}
