#include "ui/ThemeUtils.h"

#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaThemeAnimationWidget.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QEvent>
#include <QHeaderView>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QTableView>
#include <QTimer>
#include <QModelIndex>
#include <QList>
#include <QVector>

#include <cmath>
#include <numeric>

namespace
{
    class TableAutoFitHelper : public QObject
    {
    public:
        explicit TableAutoFitHelper(QTableView *table)
            : QObject(table), m_table(table)
        {
        }

        void requestResize()
        {
            scheduleResize();
        }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            Q_UNUSED(watched);
            if (!m_table)
                return QObject::eventFilter(watched, event);

            switch (event->type())
            {
            case QEvent::Show:
            case QEvent::Resize:
            case QEvent::LayoutRequest:
            case QEvent::StyleChange:
            case QEvent::PolishRequest:
                scheduleResize();
                break;
            default:
                break;
            }

            return QObject::eventFilter(watched, event);
        }

    private:
        void scheduleResize()
        {
            if (m_pending || !m_table)
                return;
            m_pending = true;
            QPointer<QTableView> tableGuard = m_table;
            QTimer::singleShot(0, this, [this, tableGuard]()
                               {
                               if (!tableGuard)
                               {
                                   m_pending = false;
                                   return;
                               }
                               resizeTableToFit(tableGuard);
                               m_pending = false; });
        }

        QPointer<QTableView> m_table;
        bool m_pending{false};
    };
} // namespace

ElaText *createFormLabel(const QString &text, QWidget *parent)
{
    auto *label = new ElaText(text, parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setTextStyle(ElaTextType::Body);
    return label;
}

void toggleThemeMode(QWidget *context)
{
    QWidget *targetWindow = nullptr;
    if (context)
    {
        targetWindow = context->window();
    }
    if (!targetWindow)
    {
        targetWindow = qApp->activeWindow();
    }

    ElaThemeAnimationWidget *overlay = nullptr;
    if (targetWindow)
    {
        const QImage previousFrame = targetWindow->grab(targetWindow->rect()).toImage();
        overlay = new ElaThemeAnimationWidget(targetWindow);
        overlay->move(0, 0);
        overlay->resize(targetWindow->size());
        overlay->setOldWindowBackground(previousFrame);
        overlay->raise();
        overlay->show();
    }

    const auto currentMode = eTheme->getThemeMode();
    const auto nextMode = currentMode == ElaThemeType::Light ? ElaThemeType::Dark : ElaThemeType::Light;
    if (nextMode != currentMode)
    {
        eTheme->setThemeMode(nextMode);
    }

    if (overlay)
    {
        overlay->startAnimation(450);
    }
}

void enableAutoFitScaling(QTableView *table)
{
    if (!table)
        return;

    if (table->property("_elaAutoFitHelperInstalled").toBool())
        return;

    auto *helper = new TableAutoFitHelper(table);
    table->setProperty("_elaAutoFitHelperInstalled", true);
    table->installEventFilter(helper);
    if (auto *viewport = table->viewport())
        viewport->installEventFilter(helper);
    if (auto *header = table->horizontalHeader())
    {
        header->installEventFilter(helper);
        QObject::connect(header, &QHeaderView::sectionResized, helper, [helper](int, int, int)
                         { helper->requestResize(); });
        QObject::connect(header, &QHeaderView::sectionCountChanged, helper, [helper](int, int)
                         { helper->requestResize(); });
        QObject::connect(header, &QHeaderView::geometriesChanged, helper, [helper]()
                         { helper->requestResize(); });
    }

    if (auto *model = table->model())
    {
        QObject::connect(model, &QAbstractItemModel::modelReset, helper, [helper]()
                         { helper->requestResize(); });
        QObject::connect(model, &QAbstractItemModel::layoutChanged, helper, [helper]()
                         { helper->requestResize(); });
        QObject::connect(model, &QAbstractItemModel::dataChanged, helper, [helper](const QModelIndex &, const QModelIndex &, const QList<int> &)
                         { helper->requestResize(); });
    }

    helper->requestResize();
}

void resizeTableToFit(QTableView *table)
{
    if (!table || !table->model())
        return;

    table->resizeColumnsToContents();

    auto *header = table->horizontalHeader();
    if (!header)
        return;

    const int columnCount = header->count();
    if (columnCount <= 0)
        return;

    QVector<int> baseWidths(columnCount);
    int contentWidth = 0;
    for (int column = 0; column < columnCount; ++column)
    {
        const int width = header->sectionSize(column);
        baseWidths[column] = width;
        contentWidth += width;
    }

    int availableWidth = table->viewport()->width();
    if (availableWidth <= 0)
    {
        const int frame = table->frameWidth() * 2;
        const int vHeaderWidth = table->verticalHeader() ? table->verticalHeader()->width() : 0;
        availableWidth = table->width() - frame - vHeaderWidth;
    }

    const int extraWidth = availableWidth - contentWidth;
    if (extraWidth <= 0)
        return;

    const double total = std::accumulate(baseWidths.cbegin(), baseWidths.cend(), 0.0);
    if (total <= 0.0)
        return;

    int remaining = extraWidth;
    for (int column = 0; column < columnCount; ++column)
    {
        const double ratio = static_cast<double>(baseWidths[column]) / total;
        int delta = static_cast<int>(std::round(ratio * extraWidth));
        if (column == columnCount - 1)
        {
            delta = remaining;
        }
        else
        {
            remaining -= delta;
        }

        const int newWidth = baseWidths[column] + std::max(0, delta);
        header->resizeSection(column, newWidth);
    }
}
