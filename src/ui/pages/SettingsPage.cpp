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
    : BasePage(QStringLiteral(u"系统设置"), QStringLiteral(u"修改软件相关设置。"), parent)
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

    auto *actionsRow = new QWidget(container);
    auto *actionsLayout = new QHBoxLayout(actionsRow);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(16);

    auto configureActionButton = [](ElaPushButton *button)
    {
        button->setMinimumHeight(44);
        button->setBorderRadius(12);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QFont f = button->font();
        if (f.pixelSize() > 0)
        {
            f.setPixelSize(f.pixelSize() + 2);
        }
        else if (f.pointSizeF() > 0.0)
        {
            f.setPointSizeF(f.pointSizeF() + 1.0);
        }
        else
        {
            f.setPointSize(11);
        }
        f.setBold(true);
        button->setFont(f);
    };

    m_changePasswordButton = new ElaPushButton(QStringLiteral(u"修改密码"), actionsRow);
    configureActionButton(m_changePasswordButton);
    actionsLayout->addWidget(m_changePasswordButton);

    m_switchAccountButton = new ElaPushButton(QStringLiteral(u"切换账号"), actionsRow);
    configureActionButton(m_switchAccountButton);
    actionsLayout->addWidget(m_switchAccountButton);

    layout->addWidget(actionsRow);

    m_backupRow = new QWidget(container);
    auto *backupLayout = new QHBoxLayout(m_backupRow);
    backupLayout->setContentsMargins(0, 0, 0, 0);
    backupLayout->setSpacing(16);

    m_backupButton = new ElaPushButton(QStringLiteral(u"导出数据备份"), m_backupRow);
    configureActionButton(m_backupButton);
    backupLayout->addWidget(m_backupButton);

    m_restoreButton = new ElaPushButton(QStringLiteral(u"导入数据备份"), m_backupRow);
    configureActionButton(m_restoreButton);
    backupLayout->addWidget(m_restoreButton);

    layout->addWidget(m_backupRow);
    m_backupRow->setVisible(false);

    layout->addStretch();

    bodyLayout()->addWidget(container);

    connect(m_darkModeSwitch, &ElaToggleSwitch::toggled, this, &SettingsPage::darkModeToggled);
    connect(m_acrylicSwitch, &ElaToggleSwitch::toggled, this, &SettingsPage::acrylicToggled);
    connect(m_changePasswordButton, &ElaPushButton::clicked, this, &SettingsPage::changePasswordRequested);
    connect(m_switchAccountButton, &ElaPushButton::clicked, this, &SettingsPage::switchAccountRequested);
    connect(m_backupButton, &ElaPushButton::clicked, this, &SettingsPage::backupRequested);
    connect(m_restoreButton, &ElaPushButton::clicked, this, &SettingsPage::restoreRequested);
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

void SettingsPage::setDataManagementVisible(bool visible)
{
    if (m_backupRow)
        m_backupRow->setVisible(visible);
    if (m_backupButton)
        m_backupButton->setEnabled(visible);
    if (m_restoreButton)
        m_restoreButton->setEnabled(visible);
}
