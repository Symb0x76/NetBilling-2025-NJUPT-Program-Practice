#pragma once

#include <QString>

class QTableView;
class QSortFilterProxyModel;

class QWidget;
class ElaText;
class ElaLineEdit;

ElaText *createFormLabel(const QString &text, QWidget *parent);
void toggleThemeMode(QWidget *context = nullptr);
void resizeTableToFit(QTableView *table);
void enableAutoFitScaling(QTableView *table);
void attachPasswordVisibilityToggle(ElaLineEdit *lineEdit);
void attachTriStateSorting(QTableView *table, QSortFilterProxyModel *proxy);
