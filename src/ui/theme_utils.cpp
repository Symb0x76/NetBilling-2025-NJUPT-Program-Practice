#include "ui/theme_utils.h"

#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaThemeAnimationWidget.h"

#include <QApplication>

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

    const auto current = eTheme->getThemeMode();
    const auto next = current == ElaThemeType::Light ? ElaThemeType::Dark : ElaThemeType::Light;
    eTheme->setThemeMode(next);

    if (overlay)
    {
        overlay->startAnimation(450);
    }
}
