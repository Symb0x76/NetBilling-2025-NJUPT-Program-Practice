#pragma once

#include <QString>

class QWidget;
class ElaText;

ElaText *createFormLabel(const QString &text, QWidget *parent);
void toggleThemeMode(QWidget *context = nullptr);
