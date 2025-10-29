#pragma once

#include <QString>

class QTableView;

class QWidget;
class ElaText;

ElaText *createFormLabel(const QString &text, QWidget *parent);
void toggleThemeMode(QWidget *context = nullptr);
void resizeTableToFit(QTableView *table);
void enableAutoFitScaling(QTableView *table);
