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
#include "deconz/dbg_trace.h"
#include "ddf_bindingeditor.h"
#include "device_descriptions.h"


DDF_ZclReportWidget::DDF_ZclReportWidget(QWidget *parent, DDF_ZclReport *rep) :
    QFrame(parent)
{
    Q_ASSERT(rep);

    report = rep;
    attrId = new QLineEdit(this);
    mfCode = new QLineEdit(this);
    dataType = new QLineEdit(this);
    minInterval = new QSpinBox(this);
    minInterval->setMinimum(0);
    minInterval->setMaximum(65535);
    maxInterval = new QSpinBox(this);
    maxInterval->setMinimum(0);
    maxInterval->setMaximum(65535);
    reportableChange = new QLineEdit(this);

    attrId->setText(QString("0x%1").arg(rep->attributeId, 4, 16, QLatin1Char('0')));
    mfCode->setText(QString("0x%1").arg(rep->manufacturerCode, 4, 16, QLatin1Char('0')));
    dataType->setText(QString("0x%1").arg(rep->dataType, 2, 16, QLatin1Char('0')));
    minInterval->setValue(rep->minInterval);
    maxInterval->setValue(rep->maxInterval);
    reportableChange->setText(QString::number(rep->reportableChange));

    connect(attrId, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::attributeIdChanged);
    connect(mfCode, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::mfCodeChanged);
    connect(dataType, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::dataTypeChanged);
    connect(reportableChange, &QLineEdit::textChanged, this, &DDF_ZclReportWidget::reportableChangeChanged);
    connect(minInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &DDF_ZclReportWidget::minMaxChanged);
    connect(maxInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &DDF_ZclReportWidget::minMaxChanged);

    auto *lay = new QFormLayout;

    lay->addRow("Manufacturer code", mfCode);
    lay->addRow("Attribute", attrId);
    lay->addRow("Datatype", dataType);
    lay->addRow("Min interval", minInterval);
    lay->addRow("Max interval", maxInterval);
    lay->addRow("Reportable change", reportableChange);

    setLayout(lay);
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    QAction *removeAction = new QAction(tr("Delete"), this);
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
        if (ok)
        {
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
        bool ok;
        uint val = dataType->text().toUShort(&ok, 0);
        if (ok && val <= UINT8_MAX)
        {
            report->dataType = val;
            emit changed();
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

void DDF_ZclReportWidget::minMaxChanged()
{
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

    d->bndModel = new QStandardItemModel(this);
    d->bndModel->setColumnCount(3);

    d->bndView = new QTableView(this);
    d->bndView->setModel(d->bndModel);
    d->bndView->horizontalHeader()->setStretchLastSection(true);
    d->bndView->setMaximumWidth(400);
    d->bndView->setSelectionBehavior(QTableView::SelectRows);
    d->bndView->verticalHeader()->hide();
    connect(d->bndView->selectionModel(), &QItemSelectionModel::currentChanged, this, &DDF_BindingEditor::bindingActivated);

    d->bndView->setAcceptDrops(true);
    d->bndView->installEventFilter(this);
    lay->addWidget(d->bndView);

    d->repScrollArea = new QScrollArea(this);
    d->repWidget = new QWidget;
    d->repWidget->installEventFilter(this);
    d->repWidget->setAcceptDrops(true);
    auto *scrollLay = new QVBoxLayout;
    scrollLay->addStretch();
    d->repWidget->setLayout(scrollLay);

    d->repScrollArea->setWidget(d->repWidget);
    d->repScrollArea->setWidgetResizable(true);

    lay->addWidget(d->repScrollArea);


    lay->addStretch();
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
        QStandardItem *type = new QStandardItem((bnd.isUnicastBinding ? "unicast" : "group"));
        QStandardItem *srcEp = new QStandardItem(QString("0x%1").arg(bnd.srcEndpoint, 2, 16, QLatin1Char('0')));
        QStandardItem *cluster = new QStandardItem(QString("0x%1").arg(bnd.clusterId, 4, 16, QLatin1Char('0')));

        d->bndModel->appendRow({type,srcEp,cluster});
    }
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

        if (object == d->bndView)
        {
            if (urls.first().scheme() == QLatin1String("cluster"))
            {
                e->accept();
                return true;
            }
        }
        else if (object == d->repWidget)
        {
            QModelIndex index;
            DDF_Binding *bnd = d->getSelectedBinding(&index);

            if (bnd && urls.first().scheme() == QLatin1String("zclattr"))
            {
                QUrlQuery urlQuery(urls.first());

                if (urlQuery.queryItemValue("cl").toUInt() == bnd->clusterId)
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
    if (!index.isValid() || index.row() >= int(d->bindings.size()))
    {
        return;
    }

    for (auto *w : d->repReportWidgets)
    {
        w->report = nullptr;
        w->hide();
        w->deleteLater();
    }

    d->repReportWidgets.clear();

    auto &bnd = d->bindings[index.row()];
    QVBoxLayout *lay = static_cast<QVBoxLayout*>(d->repWidget->layout());

    int i = 0;
    for (auto &rep : bnd.reporting)
    {
        auto *w = new DDF_ZclReportWidget(d->repWidget, &rep);

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
    bnd.srcEndpoint = urlQuery.queryItemValue("ep").toUInt() & 0xFF;
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
    rep.attributeId = urlQuery.queryItemValue("a").toUShort(&ok);

    if (urlQuery.hasQueryItem("mf"))
    {
        rep.manufacturerCode = urlQuery.queryItemValue("mf").toUShort();
    }

    if (urlQuery.hasQueryItem("dt"))
    {
        rep.dataType = urlQuery.queryItemValue("dt").toUShort() & 0xFF;
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
