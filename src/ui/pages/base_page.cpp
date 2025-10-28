#include "ui/pages/base_page.h"

#include "ElaText.h"

#include <QWidget>
#include <QFont>
#include <QVBoxLayout>

BasePage::BasePage(const QString& title, const QString& description, QWidget* parent)
    : ElaScrollPage(parent)
    , m_bodyLayout(new QVBoxLayout)
    , m_descriptionLabel(nullptr)
{
    auto* page = new QWidget(this);
    page->setWindowTitle(title);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* titleLabel = new ElaText(title, page);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(24);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);

    layout->addWidget(titleLabel);

    m_descriptionLabel = new ElaText(description, page);
    m_descriptionLabel->setTextPixelSize(13);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setVisible(!description.isEmpty());
    layout->addWidget(m_descriptionLabel);

    auto* container = new QWidget(page);
    container->setContentsMargins(0, 0, 0, 0);
    container->setLayout(m_bodyLayout);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(16);

    layout->addWidget(container);
    layout->addStretch();

    addCentralWidget(page);
}

void BasePage::setDescription(const QString& description)
{
    m_descriptionLabel->setText(description);
    m_descriptionLabel->setVisible(!description.isEmpty());
}
