#include "ui/pages/settings_page.h"

#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToggleSwitch.h"

#include <QHBoxLayout>
#include <QColor>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
    QPixmap createRoundedAvatar(const QPixmap &source, int size)
    {
        QPixmap scaled = source;
        if (scaled.isNull())
        {
            scaled = QPixmap(size, size);
            scaled.fill(QColor(0xD0, 0xD0, 0xD0));
        }

        scaled = scaled.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

        QPixmap result(size, size);
        result.fill(Qt::transparent);

        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addEllipse(0, 0, size, size);
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, scaled);
        painter.end();

        return result;
    }

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

    auto *avatarWrapper = new QWidget(container);
    auto *avatarWrapperLayout = new QVBoxLayout(avatarWrapper);
    avatarWrapperLayout->setContentsMargins(0, 0, 0, 0);
    avatarWrapperLayout->setSpacing(8);

    auto *avatarRow = new QWidget(avatarWrapper);
    auto *avatarRowLayout = new QHBoxLayout(avatarRow);
    avatarRowLayout->setContentsMargins(0, 0, 0, 0);
    avatarRowLayout->setSpacing(12);

    m_avatarPreview = new QLabel(avatarRow);
    m_avatarPreview->setFixedSize(m_avatarPreviewSize, m_avatarPreviewSize);
    m_avatarPreview->setAlignment(Qt::AlignCenter);
    m_avatarPreview->setPixmap(createRoundedAvatar(QPixmap(), m_avatarPreviewSize));
    avatarRowLayout->addWidget(m_avatarPreview, 0, Qt::AlignVCenter);

    auto *avatarInfoLayout = new QVBoxLayout();
    avatarInfoLayout->setContentsMargins(0, 0, 0, 0);
    avatarInfoLayout->setSpacing(6);

    auto *avatarTitle = new ElaText(QStringLiteral(u"个人头像"), avatarRow);
    avatarTitle->setTextStyle(ElaTextType::Subtitle);
    avatarInfoLayout->addWidget(avatarTitle, 0, Qt::AlignLeft);

    auto *avatarDesc = new ElaText(QStringLiteral(u"更新显示在导航栏顶部的头像图片。"), avatarRow);
    avatarDesc->setTextStyle(ElaTextType::Body);
    avatarDesc->setTextPixelSize(12);
    avatarDesc->setWordWrap(true);
    avatarInfoLayout->addWidget(avatarDesc, 0, Qt::AlignLeft);

    avatarInfoLayout->addStretch();

    m_changeAvatarButton = new ElaPushButton(QStringLiteral(u"更换头像"), avatarRow);
    m_changeAvatarButton->setFixedHeight(32);
    m_changeAvatarButton->setBorderRadius(8);
    avatarInfoLayout->addWidget(m_changeAvatarButton, 0, Qt::AlignLeft);

    avatarRowLayout->addLayout(avatarInfoLayout, 1);
    avatarWrapperLayout->addWidget(avatarRow);

    layout->addWidget(avatarWrapper);

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
    connect(m_changeAvatarButton, &ElaPushButton::clicked, this, &SettingsPage::avatarChangeRequested);
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

void SettingsPage::setAvatarPreview(const QPixmap &pixmap)
{
    if (!m_avatarPreview)
        return;
    m_avatarPreview->setPixmap(createRoundedAvatar(pixmap, m_avatarPreviewSize));
}
