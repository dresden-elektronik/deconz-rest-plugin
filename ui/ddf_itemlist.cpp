#include <QAbstractListModel>
#include <QMimeData>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QUrl>
#include "deconz/dbg_trace.h"
#include "device_descriptions.h"
#include "ddf_itemlist.h"

const char *ddfMimeItemName = "ddf/itemname";

#define ITEM_TYPE_ROLE (Qt::UserRole + 2)
#define ITEM_TYPE_SUBDEVICE 0
#define ITEM_TYPE_DDF_ITEM_CAP     1
#define ITEM_TYPE_DDF_ITEM_CONFIG  2
#define ITEM_TYPE_DDF_ITEM_ATTR    3
#define ITEM_TYPE_DDF_ITEM_STATE   4

struct ItemDrawOptions
{
    QColor bgColor;
    QColor fgColor;
};

static const ItemDrawOptions itemDrawOptions[] =
{

    { QColor(100, 100, 100), QColor(255, 255, 255) },  // I_TYPE_SUBDEVICE
    { QColor(224, 119, 119), QColor(0, 0, 0) },        // I_TYPE_ITEM_CAP
    { QColor(187, 222, 251), QColor(0, 0, 0) },        // I_TYPE_ITEM_CONFIG
    { QColor(218, 209, 238), QColor(0, 0, 0) },        // I_TYPE_ITEM_ATTR
    { QColor(190, 238, 194), QColor(0, 0, 0) }         // I_TYPE_ITEM_STATE
};

class ItemDelegate : public QStyledItemDelegate
{
public:
    ItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int type = index.data(ITEM_TYPE_ROLE).toInt();

    if (type >= 0 && type <= ITEM_TYPE_DDF_ITEM_STATE)
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QColor bgColor = itemDrawOptions[type].bgColor;
        QColor fgColor = itemDrawOptions[type].fgColor;

        if (opt.state.testFlag(QStyle::State_MouseOver))
        {
            bgColor = bgColor.lighter(104);
        }

        opt.backgroundBrush = QBrush(bgColor);
        painter->fillRect(opt.rect, opt.backgroundBrush);

        painter->setPen(bgColor.lighter(118)); // top light edge
        painter->drawLine(opt.rect.topLeft(), opt.rect.topRight());

        painter->setPen(bgColor.darker(170)); // bottom shadow edge
        painter->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());

        painter->setPen(fgColor);
        opt.rect.setLeft(opt.rect.left() + 4);
        painter->drawText(opt.rect, Qt::AlignVCenter, index.data().toString());
    }
    else
    {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

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
            int type = idx.data(ITEM_TYPE_ROLE).toInt();
            if (type == ITEM_TYPE_DDF_ITEM_CAP || type == ITEM_TYPE_DDF_ITEM_CONFIG || type == ITEM_TYPE_DDF_ITEM_STATE || type == ITEM_TYPE_DDF_ITEM_ATTR)
            {
                url.setScheme("ddfitem");
            }
            else if (type == ITEM_TYPE_SUBDEVICE)
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
    setItemDelegate(new ItemDelegate(this));
    setMouseTracking(true);
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
            int type = -1;

            switch (i.name.c_str()[0])
            {
            case 'a': type = ITEM_TYPE_DDF_ITEM_ATTR; break;
            case 'c': type = i.name.c_str()[1] == 'a' ? ITEM_TYPE_DDF_ITEM_CAP : ITEM_TYPE_DDF_ITEM_CONFIG; break;
            case 's': type = ITEM_TYPE_DDF_ITEM_STATE; break;
            default:
                break;
            }

            if (type == -1)
            {
                continue;
            }

            QStandardItem *item = new QStandardItem(i.name.c_str());
            item->setToolTip(i.description);
            item->setData(type, ITEM_TYPE_ROLE);
            d->model->appendRow(item);
        }
    }
}
