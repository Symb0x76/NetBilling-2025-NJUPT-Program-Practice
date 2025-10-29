#pragma once

#include <QObject>
#include <QPointer>
#include <QMargins>

class ElaNavigationBar;
class ElaWindow;
class ElaToolButton;
class ElaText;
class QTreeView;
class QListView;
class QWidget;
class QHBoxLayout;
class QBoxLayout;
class QLayout;

class NavigationPaneController : public QObject
{
    Q_OBJECT

public:
    NavigationPaneController(ElaNavigationBar *navigationBar, ElaWindow *window);
    void apply();

private:
    void initialize();
    void rebuildLayout();
    void hideBuiltInElements();
    void syncDisplayMode();
    void ensureConnections();
    ElaToolButton *locateToggleButton() const;
    void toggleDisplayMode();

    QPointer<ElaNavigationBar> m_navigationBar;
    QPointer<ElaWindow> m_window;
    QPointer<ElaToolButton> m_toggleButton;
    QPointer<ElaText> m_titleLabel;
    QPointer<QWidget> m_headerWidget;
    QPointer<QTreeView> m_navigationView;
    QPointer<QListView> m_footerView;
    QPointer<QBoxLayout> m_rootLayout;
    QPointer<QHBoxLayout> m_headerLayout;
    QPointer<ElaToolButton> m_originalToggleButton;
    bool m_initialized{false};
    QMargins m_defaultMargins;
    int m_defaultSpacing{-1};
    bool m_defaultMarginsCaptured{false};
};
