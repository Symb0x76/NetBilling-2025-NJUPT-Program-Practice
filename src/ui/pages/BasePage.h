#pragma once

#include <ElaScrollPage.h>

#include <QString>

class QShowEvent;

class QVBoxLayout;
class ElaText;

class BasePage : public ElaScrollPage
{
    Q_OBJECT

public:
    explicit BasePage(const QString &title, const QString &description = QString(), QWidget *parent = nullptr);
    ~BasePage() override = default;

protected:
    QVBoxLayout *bodyLayout() const { return m_bodyLayout; }
    void setDescription(const QString &description);
    void showEvent(QShowEvent *event) override;
    virtual void reloadPageData();

private:
    QVBoxLayout *m_bodyLayout;
    ElaText *m_descriptionLabel;
};
