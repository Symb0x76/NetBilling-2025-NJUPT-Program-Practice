#include "ui/ThemeUtils.h"

#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaThemeAnimationWidget.h"

#include <QApplication>
#include <QHeaderView>
#include <QImage>
#include <QTableView>
#include <QVector>

#include <cmath>
#include <numeric>

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

void resizeTableToFit(QTableView *table)
{
    if (!table || !table->model())
        return;

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
