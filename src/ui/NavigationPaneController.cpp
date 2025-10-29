#include "ui/NavigationPaneController.h"

#include "ElaNavigationBar.h"
#include "ElaToolButton.h"
#include "ElaText.h"
#include "ElaWindow.h"
#include "ElaDef.h"
#include "ElaSuggestBox.h"

#include <QBoxLayout>
#include <QLayout>
#include <QMargins>
#include <QVariant>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSize>
#include <QString>
#include <QTreeView>
#include <QListView>

namespace
{
    constexpr int kCompactButtonSize = 36;
    constexpr int kExpandedButtonSize = 44;
    constexpr int kHeaderHeight = 64;
}

NavigationPaneController::NavigationPaneController(ElaNavigationBar *navigationBar, ElaWindow *window)
    : QObject(window), m_navigationBar(navigationBar), m_window(window)
{
}

void NavigationPaneController::apply()
{
    if (!m_navigationBar)
    {
        return;
    }

    if (!m_initialized)
    {
        initialize();
        m_initialized = true;
    }

    syncDisplayMode();
}

void NavigationPaneController::initialize()
{
    hideBuiltInElements();
    rebuildLayout();
    ensureConnections();
}

void NavigationPaneController::rebuildLayout()
{
    if (!m_navigationBar)
    {
        return;
    }

    m_navigationView = m_navigationBar->findChild<QTreeView *>();
    m_footerView = m_navigationBar->findChild<QListView *>();
    m_originalToggleButton = locateToggleButton();

    if (m_originalToggleButton)
    {
        m_originalToggleButton->setVisible(false);
        m_originalToggleButton->setEnabled(false);
        m_originalToggleButton->setFixedSize(QSize(0, 0));
        m_originalToggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    auto *existingLayout = qobject_cast<QVBoxLayout *>(m_navigationBar->layout());
    if (!existingLayout)
    {
        return;
    }

    m_rootLayout = existingLayout;
    if (!m_defaultMarginsCaptured)
    {
        m_defaultMargins = m_rootLayout->contentsMargins();
        m_defaultSpacing = m_rootLayout->spacing();
        if (m_defaultSpacing < 8)
        {
            m_defaultSpacing = 8;
        }
        m_defaultMarginsCaptured = true;
    }

    if (m_rootLayout->contentsMargins() != m_defaultMargins)
    {
        m_rootLayout->setContentsMargins(m_defaultMargins);
    }
    if (m_defaultSpacing >= 0 && m_rootLayout->spacing() != m_defaultSpacing)
    {
        m_rootLayout->setSpacing(m_defaultSpacing);
    }

    if (!m_headerWidget)
    {
        m_headerWidget = new QWidget(m_navigationBar);
        m_headerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_headerLayout = new QHBoxLayout(m_headerWidget);
        m_headerLayout->setContentsMargins(0, 0, 0, 0);
        m_headerLayout->setSpacing(8);

        m_toggleButton = new ElaToolButton(m_headerWidget);
        m_toggleButton->setElaIcon(ElaIconType::Bars);
        m_toggleButton->setBorderRadius(12);
        m_toggleButton->setFixedSize(kExpandedButtonSize, kExpandedButtonSize);
        m_toggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_headerLayout->addWidget(m_toggleButton);
        connect(m_toggleButton, &ElaToolButton::clicked, this, &NavigationPaneController::toggleDisplayMode);

        m_titleLabel = new ElaText(m_headerWidget);
        m_titleLabel->setText(QStringLiteral(u"NetBilling"));
        m_titleLabel->setTextStyle(ElaTextType::Title);
        m_titleLabel->setTextPixelSize(18);
        m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_headerLayout->addWidget(m_titleLabel);
        m_headerLayout->addStretch();
        m_headerWidget->setFixedHeight(kHeaderHeight);
    }

    if (existingLayout->indexOf(m_headerWidget) == -1)
    {
        existingLayout->insertWidget(0, m_headerWidget);
    }

    if (m_navigationView)
    {
        m_navigationView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        existingLayout->setStretchFactor(m_navigationView, 1);
    }
}

void NavigationPaneController::hideBuiltInElements()
{
    if (!m_navigationBar)
    {
        return;
    }

    if (auto *suggestBox = m_navigationBar->findChild<ElaSuggestBox *>())
    {
        suggestBox->hide();
        suggestBox->setMaximumSize(QSize(0, 0));
        suggestBox->setMinimumSize(QSize(0, 0));
    }

    const auto childWidgets = m_navigationBar->findChildren<QWidget *>();
    for (auto *widget : childWidgets)
    {
        if (!widget)
        {
            continue;
        }

        const QString className = QString::fromLatin1(widget->metaObject()->className());
        if (className == QLatin1String("ElaInteractiveCard") || className == QLatin1String("ElaIconButton"))
        {
            widget->hide();
            widget->setMaximumSize(QSize(0, 0));
            widget->setMinimumSize(QSize(0, 0));
            widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }
    }

    const auto buttons = m_navigationBar->findChildren<ElaToolButton *>();
    for (auto *button : buttons)
    {
        if (!button)
        {
            continue;
        }
        const QVariant iconProperty = button->property("ElaIconType");
        if (!iconProperty.isValid())
        {
            continue;
        }
        const QChar iconChar = iconProperty.canConvert<QChar>() ? iconProperty.value<QChar>() : QChar(iconProperty.toInt());
        const int iconCode = iconChar.unicode();
        if (iconCode == static_cast<int>(ElaIconType::MagnifyingGlass))
        {
            button->hide();
            button->setFixedSize(QSize(0, 0));
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }
    }
}

void NavigationPaneController::syncDisplayMode()
{
    if (!m_navigationBar)
    {
        return;
    }

    const auto mode = m_window ? m_window->getNavigationBarDisplayMode() : ElaNavigationType::NavigationDisplayMode::Maximal;
    const bool compact = (mode == ElaNavigationType::NavigationDisplayMode::Compact) || (mode == ElaNavigationType::NavigationDisplayMode::Minimal);

    if (m_rootLayout && m_defaultMarginsCaptured)
    {
        if (m_rootLayout->contentsMargins() != m_defaultMargins)
        {
            m_rootLayout->setContentsMargins(m_defaultMargins);
        }
        if (m_defaultSpacing >= 0 && m_rootLayout->spacing() != m_defaultSpacing)
        {
            m_rootLayout->setSpacing(m_defaultSpacing);
        }
    }

    if (m_headerLayout)
    {
        const QMargins headerMargins = compact ? QMargins(0, 12, 0, 12) : QMargins(0, 8, 0, 16);
        if (m_headerLayout->contentsMargins() != headerMargins)
        {
            m_headerLayout->setContentsMargins(headerMargins);
        }
        m_headerLayout->setSpacing(8);
        if (m_toggleButton)
        {
            m_headerLayout->setAlignment(m_toggleButton, compact ? Qt::AlignHCenter : Qt::AlignLeft);
            m_toggleButton->setFixedSize(compact ? QSize(kCompactButtonSize, kCompactButtonSize) : QSize(kExpandedButtonSize, kExpandedButtonSize));
        }
    }

    if (m_titleLabel)
    {
        m_titleLabel->setVisible(!compact);
    }
}

void NavigationPaneController::ensureConnections()
{
    if (!m_window)
    {
        return;
    }

    connect(m_window, &ElaWindow::pNavigationBarDisplayModeChanged, this, &NavigationPaneController::syncDisplayMode, Qt::UniqueConnection);
}

ElaToolButton *NavigationPaneController::locateToggleButton() const
{
    if (!m_navigationBar)
    {
        return nullptr;
    }

    const auto buttons = m_navigationBar->findChildren<ElaToolButton *>();
    for (auto *button : buttons)
    {
        if (!button)
        {
            continue;
        }
        const QVariant iconProperty = button->property("ElaIconType");
        if (!iconProperty.isValid())
        {
            continue;
        }
        const QChar iconChar = iconProperty.canConvert<QChar>() ? iconProperty.value<QChar>() : QChar(iconProperty.toInt());
        const int iconCode = iconChar.unicode();
        if (iconCode == static_cast<int>(ElaIconType::Bars))
        {
            return button;
        }
    }
    return nullptr;
}

void NavigationPaneController::toggleDisplayMode()
{
    if (!m_window)
    {
        return;
    }

    const auto mode = m_window->getNavigationBarDisplayMode();
    const auto nextMode = (mode == ElaNavigationType::NavigationDisplayMode::Maximal)
                              ? ElaNavigationType::NavigationDisplayMode::Compact
                              : ElaNavigationType::NavigationDisplayMode::Maximal;
    m_window->setNavigationBarDisplayMode(nextMode);
}
