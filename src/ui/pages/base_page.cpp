#include "ui/pages/base_page.h"

#include "ElaText.h"

#include <QVBoxLayout>

BasePage::BasePage(const QString& title, const QString& description, QWidget* parent)
    : ElaScrollPage(parent)
    , m_bodyLayout(new QVBoxLayout)
    , m_descriptionLabel(new ElaText(description, this))
{
    setContentsMargins(24, 24, 24, 24);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* titleLabel = new ElaText(title, this);
    titleLabel->setTextPixelSize(24);
    titleLabel->setBold(true);

    m_descriptionLabel->setTextPixelSize(13);
    m_descriptionLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    if (!description.isEmpty())
    {
        layout->addWidget(m_descriptionLabel);
    }

    auto* container = new QWidget(this);
    container->setContentsMargins(0, 0, 0, 0);
    container->setLayout(m_bodyLayout);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(16);

    layout->addWidget(container);
    layout->addStretch();
}

void BasePage::setDescription(const QString& description)
{
    m_descriptionLabel->setText(description);
    m_descriptionLabel->setVisible(!description.isEmpty());
}
