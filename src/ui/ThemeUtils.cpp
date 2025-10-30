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
#include <QFont>
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
            m_coalesceTimer.setSingleShot(true);
            m_coalesceTimer.setInterval(60);
            connect(&m_coalesceTimer, &QTimer::timeout, this, [this]()
                    { performResize(); });
        }

        void requestResize()
        {
            if (!m_table || m_inResize)
                return;

            const bool canResizeNow = m_table->isVisible() && m_table->model() && m_table->viewport() && m_table->viewport()->width() > 0;
            if (!m_hasInitialResize && canResizeNow)
            {
                performResize();
                return;
            }

            m_coalesceTimer.start();
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
                requestResize();
                break;
            default:
                break;
            }

            return QObject::eventFilter(watched, event);
        }

    private:
        void performResize()
        {
            if (!m_table || m_inResize)
                return;

            const bool ready = m_table->isVisible() && m_table->viewport() && m_table->viewport()->width() > 0;
            if (!ready)
            {
                m_coalesceTimer.start();
                return;
            }

            m_inResize = true;
            resizeTableToFit(m_table);
            m_inResize = false;
            m_hasInitialResize = true;
        }

        QPointer<QTableView> m_table;
        QTimer m_coalesceTimer;
        bool m_inResize{false};
        bool m_hasInitialResize{false};
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

    auto ensureUsableFont = [](QFont &font)
    {
        if (font.pixelSize() <= 0 && font.pointSize() <= 0)
            font.setPointSize(10);
    };

    QFont tableFont = table->font();
    ensureUsableFont(tableFont);
    table->setFont(tableFont);

    if (auto *header = table->horizontalHeader())
    {
        QFont headerFont = header->font();
        ensureUsableFont(headerFont);
        header->setFont(headerFont);
    }

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
