#include <QAction>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QItemSelectionModel>
#include <QMimeData>
#include <QUrl>
#include <QScrollBar>
#include <deconz/dbg_trace.h>
#include "ddf_treeview.h"
#include "device_descriptions.h"

#define MODEL_HANDLE_ROLE (Qt::UserRole + 2)

#define I_TYPE_DEVICE        0
#define I_TYPE_SUBDEVICE     1
#define I_TYPE_ATTR          2
#define I_TYPE_CAP           3
#define I_TYPE_CONFIG        4
#define I_TYPE_STATE         5
#define I_TYPE_ITEM_ATTR     6
#define I_TYPE_ITEM_CAP      7
#define I_TYPE_ITEM_CONFIG   8
#define I_TYPE_ITEM_STATE    9

#define I_TYPE_MAX          10

union TreeItemHandle
{
    struct
    {
        unsigned int type : 8;
        unsigned int subDevice : 8;
        unsigned int item : 8;
        unsigned int pad : 8;
    };
    uint32_t value;
};

struct ItemDrawOptions
{
    QColor bgColor;
    QColor fgColor;
};

static const ItemDrawOptions itemDrawOptions[] =
{
    { QColor(90, 90, 90), QColor(255, 255, 255) },  // I_TYPE_DEVICE
    { QColor(100, 100, 100), QColor(255, 255, 255) },  // I_TYPE_SUBDEVICE
    { QColor(193, 175, 229), QColor(0, 0, 0) },  // I_TYPE_ATTR
    { QColor(189, 98, 98), QColor(0, 0, 0) },    // I_TYPE_CAP
    { QColor(162, 204, 239), QColor(0, 0, 0) },  // I_TYPE_CONFIG
    { QColor(155, 220, 169), QColor(0, 0, 0) },  // I_TYPE_STATE
    { QColor(218, 209, 238), QColor(0, 0, 0) },  // I_TYPE_ITEM_ATTR
    { QColor(224, 119, 119), QColor(0, 0, 0) },  // I_TYPE_ITEM_CAP
    { QColor(187, 222, 251), QColor(0, 0, 0) },  // I_TYPE_ITEM_CONFIG
    { QColor(190, 238, 194), QColor(0, 0, 0) }   // I_TYPE_ITEM_STATE
};

class GridItemDelegate : public QStyledItemDelegate
{
public:
    GridItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

void GridItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{

    TreeItemHandle handle;
    handle.value = index.data(MODEL_HANDLE_ROLE).toUInt();

    if (handle.type < I_TYPE_MAX)
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QColor bgColor = itemDrawOptions[handle.type].bgColor;
        QColor fgColor = itemDrawOptions[handle.type].fgColor;

        if (opt.state.testFlag(QStyle::State_Selected))
        {
            bgColor = QColor(255, 225, 105); // egg yellow
            fgColor = QColor(Qt::black);
        }
        else if (opt.state.testFlag(QStyle::State_MouseOver))
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


DDF_TreeView::DDF_TreeView(QWidget *parent) :
    QTreeView(parent)
{
    setItemDelegate(new GridItemDelegate(this));
    setDragDropMode(QTreeView::DropOnly);
    setMouseTracking(true);

    m_model = new QStandardItemModel(this);
    setModel(m_model);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &DDF_TreeView::currentIndexChanged);

    m_removeAction = new QAction(tr("Remove"), this);
    m_removeAction->setShortcut(QKeySequence::Delete);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(m_removeAction, &QAction::triggered, this, &DDF_TreeView::removeActionTriggered);
    addAction(m_removeAction);

    setStyleSheet("QTreeView::item { padding-bottom: 2px; }");
}

void DDF_TreeView::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const QStringList formats = mimeData->formats();

    if (mimeData->hasUrls())
    {
        const auto urls = mimeData->urls();

        for (const QUrl &url : urls)
        {
            if (url.scheme() == QLatin1String("ddfitem") || url.scheme() == QLatin1String("subdevice"))
            {
                event->accept();
                break;
            }
            else
            {
                DBG_Printf(DBG_INFO, "url: %s\n", qPrintable(url.toString()));
            }
        }
    }
}

void DDF_TreeView::dragMoveEvent(QDragMoveEvent *event)
{
    QModelIndex index = indexAt(event->pos());

    if (index.isValid())
    {
//        TreeItemHandle handle;
//        handle.value = index.data(MODEL_HANDLE_ROLE).toUInt();

    }
}

void DDF_TreeView::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls())
    {
        return;
    }

    const QUrl url = event->mimeData()->urls().first();

    if (url.scheme() == QLatin1String("ddfitem"))
    {
        QModelIndex index = indexAt(event->pos());

        if (!index.isValid())
        {
            return;
        }

        TreeItemHandle handle;
        handle.value = index.data(MODEL_HANDLE_ROLE).toUInt();

        const QString suffix = url.path();
        if (!suffix.isEmpty())
        {
            emit addItem(handle.subDevice, suffix);
        }
    }
    else if (url.scheme() == QLatin1String("subdevice"))
    {
        emit addSubDevice(url.path());
    }
}

void DDF_TreeView::resizeEvent(QResizeEvent *event)
{
    QTreeView::resizeEvent(event);
}

void DDF_TreeView::removeActionTriggered()
{
    const QModelIndexList indexes = selectedIndexes();
    if (indexes.size() != 1)
    {
        return;
    }

    const QModelIndex index = indexes.first();

    TreeItemHandle handle;
    handle.value = index.data(MODEL_HANDLE_ROLE).toUInt();

    switch (handle.type)
    {
    case I_TYPE_ITEM_ATTR:
    case I_TYPE_ITEM_CAP:
    case I_TYPE_ITEM_CONFIG:
    case I_TYPE_ITEM_STATE:
    {
        emit removeItem(handle.subDevice, handle.item);
    }
        break;

    case I_TYPE_SUBDEVICE:
    {
        emit removeSubDevice(handle.subDevice);
    }
        break;

    default:
        break;
    }
}

void DDF_TreeView::currentIndexChanged(const QModelIndex &current, const QModelIndex &prev)
{
    Q_UNUSED(prev)
    TreeItemHandle handle;
    handle.value = current.data(MODEL_HANDLE_ROLE).toUInt();
    m_removeAction->setEnabled(false);

    switch (handle.type)
    {
    case I_TYPE_ITEM_ATTR:
    case I_TYPE_ITEM_CAP:
    case I_TYPE_ITEM_CONFIG:
    case I_TYPE_ITEM_STATE:
    {
        m_removeAction->setEnabled(true);
        emit itemSelected(handle.subDevice, handle.item);
    }
        break;

    case I_TYPE_SUBDEVICE:
    {
        m_removeAction->setEnabled(true);
        emit subDeviceSelected(handle.subDevice);
    }
        break;

    case I_TYPE_DEVICE:
    {
        emit deviceSelected();
    }
        break;

    default:
        break;
    }
}

void DDF_TreeView::setDDF(const DeviceDescription &ddf)
{
    int scrollPos = verticalScrollBar()->value();
    bool showImplicit = true;

    m_model->clear();

    setHeaderHidden(true);
    setIndentation(0);

    TreeItemHandle handle;
    handle.value = 0;
    handle.type = I_TYPE_DEVICE;

    QStandardItem *top = new QStandardItem(tr("Device"));

    top->setEditable(false);
    top->setData(handle.value, MODEL_HANDLE_ROLE);
    top->setForeground(QBrush(itemDrawOptions[handle.type].fgColor));
    top->setSizeHint(QSize(200, 32));

    m_model->appendRow(top);

    for (const DeviceDescription::SubDevice &sub : ddf.subDevices)
    {
        handle.type = I_TYPE_SUBDEVICE;
        handle.item = 0;

        QString subType = DeviceDescriptions::instance()->constantToString(sub.type);

        QStandardItem *isub = new QStandardItem(QString("%1 (%2)").arg(subType).arg(handle.subDevice + 1));
        isub->setEditable(false);
        isub->setData(handle.value, MODEL_HANDLE_ROLE);
        isub->setForeground(QBrush(itemDrawOptions[handle.type].fgColor));

        top->appendRow(isub);

        QStandardItem *cap = nullptr;
        QStandardItem *config = nullptr;
        QStandardItem *attr = nullptr;
        QStandardItem *state = nullptr;

        for (const DeviceDescription::Item &item : sub.items)
        {
            if (!showImplicit && item.isImplicit)
            {
                continue;
            }

            QStandardItem *iParent = nullptr;

            if (item.name.c_str()[0] == 'a')
            {
                if (!attr)
                {
                    handle.type = I_TYPE_ATTR;
                    attr = new QStandardItem(QLatin1String("Attributes"));
                    attr->setData(handle.value, MODEL_HANDLE_ROLE);
                    attr->setEditable(false);
                    attr->setDragEnabled(false);
                    attr->setSelectable(false);
                    isub->appendRow(attr);
                }

                handle.type = I_TYPE_ITEM_ATTR;
                iParent = attr;
            }
            else if (item.name.c_str()[0] == 'c')
            {
                if (item.name.c_str()[1] == 'a')
                {
                    if (!cap)
                    {
                        handle.type = I_TYPE_CAP;
                        cap = new QStandardItem(QLatin1String("Capabilities"));
                        cap->setData(handle.value, MODEL_HANDLE_ROLE);
                        cap->setDragEnabled(false);
                        cap->setEditable(false);
                        cap->setSelectable(false);
                        isub->appendRow(cap);
                    }

                    handle.type = I_TYPE_ITEM_CAP;
                    iParent = cap;
                }
                else
                {
                    if (!config)
                    {
                        handle.type = I_TYPE_CONFIG;
                        config = new QStandardItem(QLatin1String("Config"));
                        config->setData(handle.value, MODEL_HANDLE_ROLE);
                        config->setDragEnabled(false);
                        config->setEditable(false);
                        config->setSelectable(false);
                        isub->appendRow(config);
                    }

                    handle.type = I_TYPE_ITEM_CONFIG;
                    iParent = config;
                }
            }
            else if (item.name.c_str()[0] == 's')
            {
                if (!state)
                {
                    handle.type = I_TYPE_STATE;
                    state = new QStandardItem(QLatin1String("State"));
                    state->setData(handle.value, MODEL_HANDLE_ROLE);
                    state->setDragEnabled(false);
                    state->setEditable(false);
                    state->setSelectable(false);
                    isub->appendRow(state);
                }

                handle.type = I_TYPE_ITEM_STATE;
                iParent = state;
            }

            if (!iParent)
            {
                continue;
            }

            const char *iname = strchr(item.name.c_str(), '/');
            if (!iname)
            {
                continue;
            }
            iname++;

            QStandardItem *iitem0 = new QStandardItem(QLatin1String(iname));
            iitem0->setEditable(false);
            iitem0->setData(handle.value, MODEL_HANDLE_ROLE);
            iParent->appendRow(iitem0);

            handle.item++;
        }

        handle.subDevice++;
    }

    expandAll();
    verticalScrollBar()->setValue(scrollPos);
}
