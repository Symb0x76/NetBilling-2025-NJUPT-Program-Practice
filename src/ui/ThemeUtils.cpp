#include "ui/ThemeUtils.h"

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaTheme.h"
#include "ElaThemeAnimationWidget.h"
#include "ElaIcon.h"

#include <QApplication>
#include <QWidgetAction>
#include <QToolButton>
#include <QAbstractItemModel>
#include <QAction>
#include <QEvent>
#include <QHeaderView>
#include <QImage>
#include <QLineEdit>
#include <QObject>
#include <QFont>
#include <QPointer>
#include <QSortFilterProxyModel>
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

    enum class SortState
    {
        None,
        Ascending,
        Descending
    };

    class TriStateSortController : public QObject
    {
    public:
        TriStateSortController(QTableView *table, QSortFilterProxyModel *proxy)
            : QObject(table), m_table(table), m_proxy(proxy)
        {
            if (!m_table || !m_proxy)
                return;

            m_header = m_table->horizontalHeader();
            if (!m_header)
                return;

            m_table->setSortingEnabled(false);
            m_header->setSortIndicatorShown(false);
            m_states.resize(m_header->count(), SortState::None);

            connect(m_header, &QHeaderView::sectionClicked, this, &TriStateSortController::handleSectionClicked);
            connect(m_header, &QHeaderView::sectionCountChanged, this, [this](int, int)
                    {
                        if (!m_header)
                            return;
                        m_states.resize(m_header->count(), SortState::None);
                        m_currentSection = -1;
                        m_header->setSortIndicatorShown(false); });
        }

    private:
        void handleSectionClicked(int logicalIndex)
        {
            if (!m_proxy || !m_header)
                return;
            if (logicalIndex < 0)
                return;

            if (m_states.size() != m_header->count())
                m_states.resize(m_header->count(), SortState::None);

            if (m_currentSection != logicalIndex)
            {
                m_currentSection = logicalIndex;
                for (auto &state : m_states)
                    state = SortState::None;
            }

            SortState &state = m_states[logicalIndex];
            state = nextState(state);

            if (state == SortState::None)
            {
                m_proxy->sort(-1);
                m_proxy->invalidate();
                m_header->setSortIndicatorShown(false);
                m_currentSection = -1;
            }
            else
            {
                const auto order = state == SortState::Ascending ? Qt::AscendingOrder : Qt::DescendingOrder;
                m_proxy->sort(logicalIndex, order);
                m_header->setSortIndicator(logicalIndex, order);
                m_header->setSortIndicatorShown(true);
            }

            for (int i = 0; i < m_states.size(); ++i)
            {
                if (i != logicalIndex)
                    m_states[i] = SortState::None;
            }
        }

        SortState nextState(SortState current) const
        {
            switch (current)
            {
            case SortState::None:
                return SortState::Ascending;
            case SortState::Ascending:
                return SortState::Descending;
            case SortState::Descending:
            default:
                return SortState::None;
            }
        }

        QPointer<QTableView> m_table;
        QPointer<QHeaderView> m_header;
        QPointer<QSortFilterProxyModel> m_proxy;
        QVector<SortState> m_states;
        int m_currentSection{-1};
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

void attachPasswordVisibilityToggle(ElaLineEdit *lineEdit)
{
    if (!lineEdit)
        return;

    if (lineEdit->property("_elaPasswordToggleInstalled").toBool())
        return;

    lineEdit->setProperty("_elaPasswordToggleInstalled", true);

    auto *button = new QToolButton(lineEdit);
    button->setCursor(Qt::PointingHandCursor);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    const int toggleHeight = lineEdit->height() - 6;
    if (toggleHeight > 0)
        button->setFixedHeight(toggleHeight);
    button->setMinimumWidth(32);
    button->setIconSize(QSize(18, 18));

    auto updateAppearance = [button](ElaThemeType::ThemeMode mode, bool visible)
    {
        const QColor iconColor = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
        const QColor hoverColor = eTheme->getThemeColor(mode, ElaThemeType::PrimaryHover);
        button->setStyleSheet(QStringLiteral(
                                  "QToolButton { border: none; padding: 0 6px; color: %1; }"
                                  "QToolButton::hover { color: %2; }")
                                  .arg(iconColor.name(), hoverColor.name()));

        const ElaIconType::IconName iconName = visible ? ElaIconType::EyeSlash : ElaIconType::Eye;
        button->setIcon(ElaIcon::getInstance()->getElaIcon(iconName, 18, iconColor));
        button->setToolTip(visible ? QStringLiteral(u"点击隐藏密码") : QStringLiteral(u"点击显示密码"));
    };

    auto *toggleAction = new QWidgetAction(lineEdit);
    toggleAction->setDefaultWidget(button);
    lineEdit->addAction(toggleAction, QLineEdit::TrailingPosition);

    const auto applyState = [lineEdit, button, updateAppearance](bool checked)
    {
        lineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        updateAppearance(eTheme->getThemeMode(), checked);
    };

    QObject::connect(button, &QToolButton::toggled, lineEdit, applyState);
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, button, [button, updateAppearance](ElaThemeType::ThemeMode mode)
                     { updateAppearance(mode, button->isChecked()); });
    const bool initiallyVisible = lineEdit->echoMode() == QLineEdit::Normal;
    button->setChecked(initiallyVisible);
    applyState(initiallyVisible);
}

void attachTriStateSorting(QTableView *table, QSortFilterProxyModel *proxy)
{
    if (!table || !proxy)
        return;

    if (table->property("_elaTriStateSortInstalled").toBool())
        return;

    table->setProperty("_elaTriStateSortInstalled", true);
    new TriStateSortController(table, proxy);
}
