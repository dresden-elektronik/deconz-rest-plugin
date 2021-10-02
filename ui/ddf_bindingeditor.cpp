#include <QAction>
#include <QEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QUrlQuery>
#include <QVBoxLayout>
#include "deconz/dbg_trace.h"
#include "deconz/zcl.h"
#include "ddf_bindingeditor.h"
#include "device_descriptions.h"


DDF_ZclReportWidget::DDF_ZclReportWidget(QWidget *parent, DDF_ZclReport *rep, const deCONZ::ZclCluster *cl) :
    QFrame(parent)
{
    Q_ASSERT(rep);
    Q_ASSERT(cl);

    cluster = cl;
    report = rep;
    attrId = new QLineEdit(this);
    attrName = new QLabel(this);
    attrName->setWordWrap(true);
    QFont smallFont = font();
    smallFont.setPointSize(smallFont.pointSize() - 1);
    mfCode = new QLineEdit(this);
    mfCode->setPlaceholderText(QLatin1String("0x0000"));
    dataType = new QLineEdit(this);
    minInterval = new QSpinBox(this);
    minInterval->setMinimum(0);
    minInterval->setMaximum(65535);
    maxInterval = new QSpinBox(this);
    maxInterval->setMinimum(0);
    maxInterval->setMaximum(65535);
    reportableChange = new QLineEdit(this);

    const auto dt = deCONZ::ZCL_DataType(rep->dataType);
    DBG_Assert(dt.isValid());

    auto at = std::find_if(cl->attributes().cbegin(), cl->attributes().cend(), [rep](const auto &i) { return i.id() == rep->attributeId; });

    attrId->setText(QString("0x%1").arg(rep->attributeId, 4, 16, QLatin1Char('0')));
    if (rep->manufacturerCode != 0)
    {
        mfCode->setText(QString("0x%1").arg(rep->manufacturerCode, 4, 16, QLatin1Char('0')));
    }

    if (at != cl->attributes().cend())
    {
        attrName->setText(at->name());
    }

    if (dt.isValid())
    {
        dataType->setText(dt.shortname());
    }
    else
    {
        dataType->setText(QString("0x%1").arg(rep->dataType, 2, 16, QLatin1Char('0')));
    }
    minInterval->setValue(rep->minInterval);
    maxInterval->setValue(rep->maxInterval);
    reportableChange->setText(QString::number(rep->reportableChange));

    connect(attrId, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::attributeIdChanged);
    connect(mfCode, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::mfCodeChanged);
    connect(dataType, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::dataTypeChanged);
    connect(reportableChange, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::reportableChangeChanged);
    connect(minInterval, SIGNAL(valueChanged(int)), this, SLOT(minMaxChanged(int)));
    connect(maxInterval, SIGNAL(valueChanged(int)), this, SLOT(minMaxChanged(int)));

    auto *lay = new QFormLayout;

    lay->addRow(QLatin1String("Attribute"), attrName);
    lay->addRow(QLatin1String("Attribute ID"), attrId);
    lay->addRow(QLatin1String("Manufacturer code"), mfCode);
    lay->addRow(QLatin1String("Datatype ID"), dataType);
    lay->addRow(QLatin1String("Min interval"), minInterval);
    lay->addRow(QLatin1String("Max interval"), maxInterval);
    lay->addRow(QLatin1String("Reportable change"), reportableChange);

    setLayout(lay);
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    QAction *removeAction = new QAction(tr("Remove"), this);
    addAction(removeAction);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(removeAction, &QAction::triggered, this, &DDF_ZclReportWidget::removed);
}

void DDF_ZclReportWidget::attributeIdChanged()
{
    if (report)
    {
        bool ok;
        uint val = attrId->text().toUShort(&ok, 0);
        if (ok && report->attributeId != val)
        {
            // update attribute name
            auto at = std::find_if(cluster->attributes().cbegin(), cluster->attributes().cend(), [val](const auto &i) { return i.id() == val; });
            if (at != cluster->attributes().cend())
            {
                attrName->setText(at->name());
            }
            else
            {
                attrName->clear();
            }

            report->attributeId = val;
            emit changed();
        }
    }
}

void DDF_ZclReportWidget::mfCodeChanged()
{
    if (report)
    {
        bool ok;
        uint val = mfCode->text().toUShort(&ok, 0);
        if (ok)
        {
            report->manufacturerCode = val;
            emit changed();
        }
    }
}

void DDF_ZclReportWidget::dataTypeChanged()
{
    if (report)
    {
        QString text = dataType->text();

        if (text.startsWith(QLatin1String("0x")))
        {
            bool ok;
            uint val = dataType->text().toUShort(&ok, 0);
            if (ok && val <= UINT8_MAX)
            {
                const auto dt = deCONZ::ZCL_DataType(val);

                if (dt.isValid())
                {
                    report->dataType = val;
                    emit changed();
                }
            }
        }
        else
        {
            const auto dt = deCONZ::ZCL_DataType(text);
            if (dt.isValid() && report->dataType != dt.id())
            {
                report->dataType = dt.id();
                emit changed();
            }
        }
    }
}

void DDF_ZclReportWidget::reportableChangeChanged()
{
    if (report)
    {
        bool ok;
        uint val = reportableChange->text().toUInt(&ok, 0);
        if (ok && val <= UINT32_MAX)
        {
            report->reportableChange = val;
            emit changed();
        }
    }
}

void DDF_ZclReportWidget::minMaxChanged(int val)
{
    Q_UNUSED(val)
    if (report)
    {
        report->minInterval = minInterval->value();
        report->maxInterval = maxInterval->value();
        emit changed();
    }
}

class DDF_BindingEditorPrivate
{
public:
    DDF_Binding *getSelectedBinding(QModelIndex *index);

    std::vector<DDF_Binding> bindings;
    QTableView *bndView = nullptr;
    QStandardItemModel *bndModel = nullptr;
    QScrollArea *repScrollArea = nullptr;
    QWidget *repWidget = nullptr;
    deCONZ::ZclCluster curCluster;

    std::vector<DDF_ZclReportWidget*> repReportWidgets;
};

DDF_Binding *DDF_BindingEditorPrivate::getSelectedBinding(QModelIndex *index)
{
    const QModelIndexList indexes = bndView->selectionModel()->selectedIndexes();
    if (indexes.isEmpty())
    {
        return nullptr;
    }

    *index = indexes.first();
    if (!index->isValid() || index->row() >= int(bindings.size()))
    {
        return nullptr;
    }

    return &bindings[index->row()];
}


DDF_BindingEditor::DDF_BindingEditor(QWidget *parent) :
    QWidget(parent),
    d(new DDF_BindingEditorPrivate)
{
    auto *lay = new QHBoxLayout;
    setLayout(lay);

    auto *bndLay = new QVBoxLayout;

    auto *bndLabel = new QLabel(tr("Bindings"));
    bndLay->addWidget(bndLabel);

    d->bndModel = new QStandardItemModel(this);
    d->bndModel->setColumnCount(3);

    d->bndView = new QTableView(this);
    d->bndView->setModel(d->bndModel);
    d->bndView->horizontalHeader()->setStretchLastSection(true);
    d->bndView->setMinimumWidth(400);
    d->bndView->setMaximumWidth(600);
    d->bndView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    d->bndView->setSelectionBehavior(QTableView::SelectRows);
    d->bndView->setSelectionMode(QTableView::SingleSelection);
    d->bndView->verticalHeader()->hide();
    d->bndView->setAcceptDrops(true);
    d->bndView->installEventFilter(this);
    connect(d->bndView->selectionModel(), &QItemSelectionModel::currentChanged, this, &DDF_BindingEditor::bindingActivated);

    QAction *removeAction = new QAction(tr("Remove"), this);
    d->bndView->addAction(removeAction);
    d->bndView->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(removeAction, &QAction::triggered, this, &DDF_BindingEditor::removeBinding);

    bndLay->addWidget(d->bndView);

    lay->addLayout(bndLay);

    auto *repLay = new QVBoxLayout;

    auto *repLabel = new QLabel(tr("Reporting configuration"));
    repLay->addWidget(repLabel);

    d->repScrollArea = new QScrollArea(this);
    d->repScrollArea->setMinimumWidth(400);
    d->repScrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    d->repWidget = new QWidget;
    d->repWidget->installEventFilter(this);
    d->repWidget->setAcceptDrops(true);
    auto *scrollLay = new QVBoxLayout;
    scrollLay->addStretch();
    d->repWidget->setLayout(scrollLay);

    d->repScrollArea->setWidget(d->repWidget);
    d->repScrollArea->setWidgetResizable(true);

    repLay->addWidget(d->repScrollArea);
    lay->addLayout(repLay);
    lay->addStretch(0);
}

DDF_BindingEditor::~DDF_BindingEditor()
{
    delete d;
}

const std::vector<DDF_Binding> &DDF_BindingEditor::bindings() const
{
    return d->bindings;
}

/*
  {
    "bind": "unicast",
    "src.ep": 2,
    "cl": "0x0001",
    "report": [ {"at": "0x0021", "dt": "0x20", "min": 7200, "max": 7200, "change": "0x00" } ]
  }

*/
void DDF_BindingEditor::setBindings(const std::vector<DDF_Binding> &bindings)
{
    d->bndModel->clear();
    d->bndModel->setHorizontalHeaderLabels({"Type", "Endpoint", "Cluster"});
    d->bindings = bindings;

    for (const auto &bnd : d->bindings)
    {
        const auto cl = deCONZ::ZCL_InCluster(HA_PROFILE_ID, bnd.clusterId, 0x0000);

        QStandardItem *type = new QStandardItem((bnd.isUnicastBinding ? "unicast" : "group"));
        QStandardItem *srcEp = new QStandardItem(QString("0x%1").arg(bnd.srcEndpoint, 2, 16, QLatin1Char('0')));

        QString name;
        if (cl.isValid())
        {
            name = cl.name();
        }
        else
        {
            name = QString("0x%1").arg(bnd.clusterId, 4, 16, QLatin1Char('0'));
        }

        QStandardItem *cluster = new QStandardItem(name);

        d->bndModel->appendRow({type,srcEp,cluster});
    }

    d->bndView->resizeColumnToContents(0);
    d->bndView->resizeColumnToContents(1);
    d->bndView->horizontalHeader()->setStretchLastSection(true);

    bindingActivated(d->bndModel->index(0, 0), QModelIndex());
}

bool DDF_BindingEditor::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::DragEnter)
    {
        QDragEnterEvent *e = static_cast<QDragEnterEvent*>(event);

        if (!e->mimeData()->hasUrls())
        {
            return false;
        }

        const auto urls = e->mimeData()->urls();
        const auto url = urls.first();

        if (object == d->bndView)
        {
            if (url.scheme() == QLatin1String("cluster") || url.scheme() == QLatin1String("zclattr"))
            {
                e->accept();
                return true;
            }
        }
        else if (object == d->repWidget)
        {
            QModelIndex index;
            DDF_Binding *bnd = d->getSelectedBinding(&index);

            if (bnd && url.scheme() == QLatin1String("zclattr"))
            {
                QUrlQuery urlQuery(url);

                bool ok;
                if (urlQuery.queryItemValue("cid").toUShort(&ok, 16) == bnd->clusterId)
                {
                    e->accept();
                    return true;
                }
            }
        }
    }
    else if (event->type() == QEvent::Drop)
    {
        QDropEvent *e = static_cast<QDropEvent*>(event);

        if (!e->mimeData()->hasUrls())
        {
            return false;
        }

        const auto urls = e->mimeData()->urls();

        if (object == d->bndView)
        {
            if (urls.first().scheme() == QLatin1String("cluster"))
            {
                dropClusterUrl(urls.first());
            }
            else if (urls.first().scheme() == QLatin1String("zclattr"))
            {
                dropClusterUrl(urls.first()); // contains also "cid" and "ep"
            }
            return true;
        }
        else if (object == d->repWidget)
        {
            if (urls.first().scheme() == QLatin1String("zclattr"))
            {
                dropAttributeUrl(urls.first());
            }
            return true;
        }
    }

    return false;
}

void DDF_BindingEditor::bindingActivated(const QModelIndex &index, const QModelIndex &prev)
{
    Q_UNUSED(prev)

    for (auto *w : d->repReportWidgets)
    {
        w->report = nullptr;
        w->hide();
        w->deleteLater();
    }

    d->repReportWidgets.clear();

    if (!index.isValid() || index.row() >= int(d->bindings.size()))
    {
        return;
    }

    auto &bnd = d->bindings[index.row()];
    d->curCluster = deCONZ::ZCL_InCluster(HA_PROFILE_ID, bnd.clusterId, 0x0000);

    QVBoxLayout *lay = static_cast<QVBoxLayout*>(d->repWidget->layout());

    int i = 0;
    for (auto &rep : bnd.reporting)
    {
        auto *w = new DDF_ZclReportWidget(d->repWidget, &rep, &d->curCluster);

        d->repReportWidgets.push_back(w);
        lay->insertWidget(i, w);
        i++;
        connect(w, &DDF_ZclReportWidget::changed, this, &DDF_BindingEditor::bindingsChanged);
        connect(w, &DDF_ZclReportWidget::removed, this, &DDF_BindingEditor::reportRemoved);
    }
}

void DDF_BindingEditor::dropClusterUrl(const QUrl &url)
{
    QUrlQuery urlQuery(url);

    DDF_Binding bnd{};

    bool ok;
    bnd.clusterId = urlQuery.queryItemValue("cid").toUShort(&ok, 16);
    bnd.srcEndpoint = urlQuery.queryItemValue("ep").toUInt(&ok, 16) & 0xFF;
    bnd.isUnicastBinding = 1;

    const auto i = std::find_if(d->bindings.cbegin(), d->bindings.cend(), [&](const auto &i)
    {
        return i.clusterId == bnd.clusterId &&
                i.srcEndpoint == bnd.srcEndpoint &&
                i.isUnicastBinding == bnd.isUnicastBinding;
    });

    if (i == d->bindings.cend())
    {
        d->bindings.push_back(bnd);
        setBindings(d->bindings);
        d->bndView->selectRow(d->bindings.size() - 1);
        emit bindingsChanged();
    }
}

void DDF_BindingEditor::dropAttributeUrl(const QUrl &url)
{
    QModelIndex index;
    DDF_Binding *bnd = d->getSelectedBinding(&index);
    if (!bnd)
    {
        return;
    }

    QUrlQuery urlQuery(url);

    if (!urlQuery.hasQueryItem("a"))
    {
        return;
    }

    DDF_ZclReport rep{};

    bool ok;
    rep.attributeId = urlQuery.queryItemValue("a").toUShort(&ok, 16);

    if (urlQuery.hasQueryItem("mf"))
    {
        rep.manufacturerCode = urlQuery.queryItemValue("mf").toUShort(&ok, 16);
    }

    if (urlQuery.hasQueryItem("dt"))
    {
        rep.dataType = urlQuery.queryItemValue("dt").toUShort(&ok, 16) & 0xFF;
    }

    if (urlQuery.hasQueryItem("rmin"))
    {
        rep.minInterval = urlQuery.queryItemValue("rmin").toUShort();
    }

    if (urlQuery.hasQueryItem("rmax"))
    {
        rep.maxInterval = urlQuery.queryItemValue("rmax").toUShort();
    }

    if (urlQuery.queryItemValue("t") == "A" && urlQuery.hasQueryItem("rchange"))
    {
        rep.reportableChange = urlQuery.queryItemValue("rchange").toUShort();
    }

    auto i = std::find_if(bnd->reporting.begin(), bnd->reporting.end(), [&](const auto &i) {
        return i.attributeId == rep.attributeId;
    });

    if (i == bnd->reporting.cend())
    {
        bnd->reporting.push_back(rep);
    }
    else
    {
        *i = rep;
    }

    bindingActivated(index, QModelIndex());
    emit bindingsChanged();
}

void DDF_BindingEditor::reportRemoved()
{
    DDF_ZclReportWidget *w = static_cast<DDF_ZclReportWidget*>(sender());

    if (!w || !w->report)
    {
        return;
    }

    QModelIndex index;
    DDF_Binding *bnd = d->getSelectedBinding(&index);

    if (!bnd)
    {
        return;
    }

    auto i = std::find_if(bnd->reporting.begin(), bnd->reporting.end(), [&](const auto &i){
        return w->report == &i;
    });

    if (i != bnd->reporting.end())
    {
        w->report = nullptr;
        bnd->reporting.erase(i);
        bindingActivated(index, QModelIndex());
        emit bindingsChanged();
    }
}

void DDF_BindingEditor::removeBinding()
{
    QModelIndex index;
    auto *bnd = d->getSelectedBinding(&index);

    if (!bnd || !index.isValid() || index.row() >= int(d->bindings.size()))
    {
        return;
    }

    d->bindings.erase(d->bindings.begin() + index.row());
    setBindings(d->bindings);
    emit bindingsChanged();
}
