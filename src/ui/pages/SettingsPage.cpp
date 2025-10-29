#include "ui/pages/SettingsPage.h"

#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToggleSwitch.h"

#include <QFont>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
    QWidget *createOptionRow(const QString &title,
                             const QString &description,
                             ElaToggleSwitch **outSwitch,
                             QWidget *parent)
    {
        auto *wrapper = new QWidget(parent);
        auto *wrapperLayout = new QVBoxLayout(wrapper);
        wrapperLayout->setContentsMargins(0, 0, 0, 0);
        wrapperLayout->setSpacing(4);

        auto *row = new QWidget(wrapper);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);

        auto *titleLabel = new ElaText(title, row);
        titleLabel->setTextStyle(ElaTextType::Subtitle);
        rowLayout->addWidget(titleLabel, 0, Qt::AlignVCenter);
        rowLayout->addStretch();

        auto *toggle = new ElaToggleSwitch(row);
        rowLayout->addWidget(toggle, 0, Qt::AlignVCenter);

        wrapperLayout->addWidget(row);

        if (!description.isEmpty())
        {
            auto *descLabel = new ElaText(description, wrapper);
            descLabel->setTextStyle(ElaTextType::Body);
            descLabel->setTextPixelSize(12);
            descLabel->setWordWrap(true);
            wrapperLayout->addWidget(descLabel);
        }

        if (outSwitch)
        {
            *outSwitch = toggle;
        }

        return wrapper;
    }
} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : BasePage(QStringLiteral(u"系统设置"), QStringLiteral(u"调整界面主题和视觉效果。"), parent)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    layout->addWidget(createOptionRow(QStringLiteral(u"夜间模式"),
                                      QStringLiteral(u"开启后界面使用深色主题，适合低光环境。"),
                                      &m_darkModeSwitch,
                                      container));

    layout->addWidget(createOptionRow(QStringLiteral(u"启用亚克力效果"),
                                      QStringLiteral(u"启用后主界面使用半透明的亚克力视觉效果。"),
                                      &m_acrylicSwitch,
                                      container));

    m_changePasswordButton = new ElaPushButton(QStringLiteral(u"修改密码"), container);
    m_changePasswordButton->setFixedHeight(32);
    m_changePasswordButton->setBorderRadius(8);
    layout->addWidget(m_changePasswordButton, 0, Qt::AlignLeft);

    layout->addStretch();

    bodyLayout()->addWidget(container);

    connect(m_darkModeSwitch, &ElaToggleSwitch::toggled, this, &SettingsPage::darkModeToggled);
    connect(m_acrylicSwitch, &ElaToggleSwitch::toggled, this, &SettingsPage::acrylicToggled);
    connect(m_changePasswordButton, &ElaPushButton::clicked, this, &SettingsPage::changePasswordRequested);
}

void SettingsPage::setDarkModeChecked(bool checked)
{
    if (!m_darkModeSwitch)
        return;
    QSignalBlocker blocker(m_darkModeSwitch);
    m_darkModeSwitch->setIsToggled(checked);
}

void SettingsPage::setAcrylicChecked(bool checked)
{
    if (!m_acrylicSwitch)
        return;
    QSignalBlocker blocker(m_acrylicSwitch);
    m_acrylicSwitch->setIsToggled(checked);
}
