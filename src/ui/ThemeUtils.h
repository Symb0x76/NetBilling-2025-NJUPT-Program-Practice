#pragma once

#include <QMessageBox>
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
int showThemedMessageBox(QWidget *parent,
                         QMessageBox::Icon icon,
                         const QString &title,
                         const QString &text,
                         QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                         QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
inline int showThemedWarning(QWidget *parent,
                             const QString &title,
                             const QString &text,
                             QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                             QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    return showThemedMessageBox(parent, QMessageBox::Warning, title, text, buttons, defaultButton);
}
inline int showThemedInformation(QWidget *parent,
                                 const QString &title,
                                 const QString &text,
                                 QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                 QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    return showThemedMessageBox(parent, QMessageBox::Information, title, text, buttons, defaultButton);
}
inline int showThemedQuestion(QWidget *parent,
                              const QString &title,
                              const QString &text,
                              QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    return showThemedMessageBox(parent, QMessageBox::Question, title, text, buttons, defaultButton);
}
