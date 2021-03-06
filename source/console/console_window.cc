//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "console/console_window.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "client/ui/client_window.h"
#include "common/ui/about_dialog.h"
#include "console/address_book_tab.h"
#include "console/console_settings.h"
#include "console/update_settings_dialog.h"
#include "updater/update_dialog.h"

namespace console {

namespace {

class LanguageAction : public QAction
{
public:
    LanguageAction(const QString& locale, QObject* parent = nullptr)
        : QAction(parent),
          locale_(locale)
    {
        setText(QLocale::languageToString(QLocale(locale).language()));
    }

    ~LanguageAction() = default;

    QString locale() const { return locale_; }

private:
    QString locale_;
    DISALLOW_COPY_AND_ASSIGN(LanguageAction);
};

class MruAction : public QAction
{
public:
    MruAction(const QString& file, QObject* parent = nullptr)
        : QAction(file, parent)
    {
        // Nothing
    }

    QString filePath() const { return text(); }

private:
    DISALLOW_COPY_AND_ASSIGN(MruAction);
};

} // namespace

ConsoleWindow::ConsoleWindow(Settings& settings,
                             common::LocaleLoader& locale_loader,
                             const QString& file_path)
    : settings_(settings),
      locale_loader_(locale_loader)
{
    ui.setupUi(this);

    createLanguageMenu(settings.locale());

    mru_.setRecentOpen(settings.recentOpen());
    mru_.setPinnedFiles(settings.pinnedFiles());

    rebuildMruMenu();

    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    ui.action_show_tray_icon->setChecked(settings.alwaysShowTrayIcon());
    ui.action_minimize_to_tray->setChecked(settings.minimizeToTray());
    ui.action_toolbar->setChecked(settings.isToolBarEnabled());
    ui.action_statusbar->setChecked(settings.isStatusBarEnabled());

    ui.status_bar->setVisible(ui.action_statusbar->isChecked());
    showTrayIcon(ui.action_show_tray_icon->isChecked());

    connect(ui.menu_recent_open, &QMenu::triggered, [this](QAction* action)
    {
        MruAction* mru_action = dynamic_cast<MruAction*>(action);
        if (!mru_action)
            return;

        openAddressBook(mru_action->filePath());
    });

    QTabBar* tab_bar = ui.tab_widget->tabBar();
    tab_bar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tab_bar, &QTabBar::customContextMenuRequested, this, &ConsoleWindow::onTabContextMenu);

    connect(ui.action_show_hide, &QAction::triggered, this, &ConsoleWindow::onShowHideToTray);
    connect(ui.action_show_tray_icon, &QAction::toggled, this, &ConsoleWindow::showTrayIcon);
    connect(ui.action_new, &QAction::triggered, this, &ConsoleWindow::onNew);
    connect(ui.action_open, &QAction::triggered, this, &ConsoleWindow::onOpen);
    connect(ui.action_save, &QAction::triggered, this, &ConsoleWindow::onSave);
    connect(ui.action_save_as, &QAction::triggered, this, &ConsoleWindow::onSaveAs);
    connect(ui.action_save_all, &QAction::triggered, this, &ConsoleWindow::onSaveAll);
    connect(ui.action_close, &QAction::triggered, this, &ConsoleWindow::onClose);
    connect(ui.action_close_all, &QAction::triggered, this, &ConsoleWindow::onCloseAll);

    connect(ui.action_address_book_properties, &QAction::triggered,
            this, &ConsoleWindow::onAddressBookProperties);

    connect(ui.action_add_computer, &QAction::triggered, this, &ConsoleWindow::onAddComputer);
    connect(ui.action_modify_computer, &QAction::triggered, this, &ConsoleWindow::onModifyComputer);

    connect(ui.action_delete_computer, &QAction::triggered,
            this, &ConsoleWindow::onDeleteComputer);

    connect(ui.action_add_computer_group, &QAction::triggered,
            this, &ConsoleWindow::onAddComputerGroup);

    connect(ui.action_modify_computer_group, &QAction::triggered,
            this, &ConsoleWindow::onModifyComputerGroup);

    connect(ui.action_delete_computer_group, &QAction::triggered,
            this, &ConsoleWindow::onDeleteComputerGroup);

    connect(ui.action_online_help, &QAction::triggered, this, &ConsoleWindow::onOnlineHelp);
    connect(ui.action_check_updates, &QAction::triggered, this, &ConsoleWindow::onCheckUpdates);
    connect(ui.action_about, &QAction::triggered, this, &ConsoleWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &ConsoleWindow::close);
    connect(ui.action_fast_connect, &QAction::triggered, this, &ConsoleWindow::onFastConnect);

    connect(ui.action_desktop_manage_connect, &QAction::triggered,
            this, &ConsoleWindow::onDesktopManageConnect);

    connect(ui.action_desktop_view_connect, &QAction::triggered,
            this, &ConsoleWindow::onDesktopViewConnect);

    connect(ui.action_file_transfer_connect, &QAction::triggered,
            this, &ConsoleWindow::onFileTransferConnect);

    connect(ui.tool_bar, &QToolBar::visibilityChanged, ui.action_toolbar, &QAction::setChecked);
    connect(ui.action_toolbar, &QAction::toggled, ui.tool_bar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.status_bar, &QStatusBar::setVisible);
    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &ConsoleWindow::onCurrentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &ConsoleWindow::onCloseTab);
    connect(ui.menu_language, &QMenu::triggered, this, &ConsoleWindow::onLanguageChanged);

    QActionGroup* session_type_group = new QActionGroup(this);

    session_type_group->addAction(ui.action_desktop_manage);
    session_type_group->addAction(ui.action_desktop_view);
    session_type_group->addAction(ui.action_file_transfer);

    switch (settings.sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            ui.action_desktop_manage->setChecked(true);
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            ui.action_desktop_view->setChecked(true);
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            ui.action_file_transfer->setChecked(true);
            break;

        default:
            break;
    }

    // Open all pinned files of address books.
    for (const auto& file : mru_.pinnedFiles())
    {
        if (QFile::exists(file))
        {
            addAddressBookTab(AddressBookTab::openFromFile(file, ui.tab_widget));
        }
        else
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Pinned address book file \"%1\" was not found.<br/>"
                                    "This file will be unpinned.").arg(file),
                                 QMessageBox::Ok);
            mru_.unpinFile(file);
        }
    }

    QString normalized_path(file_path);
    normalized_path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // If the address book is pinned, then it is already open.
    if (!normalized_path.isEmpty() && !mru_.isPinnedFile(normalized_path))
        addAddressBookTab(AddressBookTab::openFromFile(normalized_path, ui.tab_widget));

    if (ui.tab_widget->count() > 0)
        ui.tab_widget->setCurrentIndex(0);

    connect(ui.action_update_settings, &QAction::triggered, [this]()
    {
        UpdateSettingsDialog(this).exec();
    });

    if (settings_.checkUpdates())
    {
        updater::Checker* checker = new updater::Checker(this);

        checker->setUpdateServer(settings_.updateServer());
        checker->setPackageName(QStringLiteral("console"));

        connect(checker, &updater::Checker::finished, this, &ConsoleWindow::onUpdateChecked);
        connect(checker, &updater::Checker::finished, checker, &updater::Checker::deleteLater);

        checker->start();
    }
}

ConsoleWindow::~ConsoleWindow() = default;

void ConsoleWindow::onNew()
{
    addAddressBookTab(AddressBookTab::createNew(ui.tab_widget));
}

void ConsoleWindow::onOpen()
{
    QString file_path =
        QFileDialog::getOpenFileName(this,
                                     tr("Open Address Book"),
                                     settings_.lastDirectory(),
                                     tr("Aspia Address Book (*.aab)"));
    if (file_path.isEmpty())
        return;

    settings_.setLastDirectory(QFileInfo(file_path).absolutePath());
    openAddressBook(file_path);
}

void ConsoleWindow::onSave()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab && tab->save())
    {
        if (mru_.addRecentFile(tab->filePath()))
            rebuildMruMenu();

        if (!hasChangedTabs())
            ui.action_save_all->setEnabled(false);
    }
}

void ConsoleWindow::onSaveAs()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        QString old_path = tab->filePath();
        if (mru_.isPinnedFile(old_path))
        {
            QScopedPointer<AddressBookTab> duplicate_tab(tab->duplicateTab());
            if (duplicate_tab->saveAs())
            {
                const QString& new_path = duplicate_tab->filePath();
                if (mru_.addRecentFile(new_path))
                    rebuildMruMenu();

                addAddressBookTab(duplicate_tab.take());
            }
        }
        else if (tab->saveAs())
        {
            const QString& new_path = tab->filePath();
            if (mru_.addRecentFile(new_path))
                rebuildMruMenu();
        }

        if (!hasChangedTabs())
            ui.action_save_all->setEnabled(false);
    }
}

void ConsoleWindow::onSaveAll()
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
            tab->save();
    }

    if (!hasChangedTabs())
        ui.action_save_all->setEnabled(false);
}

void ConsoleWindow::onClose()
{
    onCloseTab(ui.tab_widget->currentIndex());
}

void ConsoleWindow::onCloseAll()
{
    for (int i = ui.tab_widget->count(); i >= 0; --i)
        onCloseTab(i);
}

void ConsoleWindow::onAddressBookProperties()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyAddressBook();
}

void ConsoleWindow::onAddComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->addComputer();
}

void ConsoleWindow::onModifyComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyComputer();
}

void ConsoleWindow::onDeleteComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->removeComputer();
}

void ConsoleWindow::onAddComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->addComputerGroup();
}

void ConsoleWindow::onModifyComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyComputerGroup();
}

void ConsoleWindow::onDeleteComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->removeComputerGroup();
}

void ConsoleWindow::onOnlineHelp()
{
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

void ConsoleWindow::onCheckUpdates()
{
    updater::UpdateDialog(settings_.updateServer(), QLatin1String("console"), this).exec();
}

void ConsoleWindow::onAbout()
{
    common::AboutDialog(this).exec();
}

void ConsoleWindow::onFastConnect()
{
    client::ClientWindow::connectToHost();
}

void ConsoleWindow::onDesktopManageConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer* computer = computer_item->computer();
            computer->set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);
            connectToComputer(*computer);
        }
    }
}

void ConsoleWindow::onDesktopViewConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer* computer = computer_item->computer();
            computer->set_session_type(proto::SESSION_TYPE_DESKTOP_VIEW);
            connectToComputer(*computer);
        }
    }
}

void ConsoleWindow::onFileTransferConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer* computer = computer_item->computer();
            computer->set_session_type(proto::SESSION_TYPE_FILE_TRANSFER);
            connectToComputer(*computer);
        }
    }
}

void ConsoleWindow::onCurrentTabChanged(int index)
{
    if (index == -1)
    {
        ui.action_save->setEnabled(false);
        ui.action_add_computer_group->setEnabled(false);
        ui.action_add_computer->setEnabled(false);
        return;
    }

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
        return;

    ui.action_save->setEnabled(tab->isChanged());
    ui.action_close->setEnabled(!mru_.isPinnedFile(tab->filePath()));

    proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
    if (computer_group)
        ui.status_bar->setCurrentComputerGroup(*computer_group);
    else
        ui.status_bar->clear();
}

void ConsoleWindow::onCloseTab(int index)
{
    if (index == -1)
        return;

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
        return;

    if (mru_.isPinnedFile(tab->filePath()))
        return;

    if (tab->isChanged())
    {
        int ret = QMessageBox(QMessageBox::Question,
                              tr("Confirmation"),
                              tr("Address book \"%1\" has been changed. Save changes?")
                              .arg(tab->addressBookName()),
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                              this).exec();
        switch (ret)
        {
            case QMessageBox::Yes:
                tab->save();
                break;

            case QMessageBox::Cancel:
                return;

            default:
                break;
        }
    }

    ui.tab_widget->removeTab(index);
    delete tab;

    if (!ui.tab_widget->count())
    {
        ui.status_bar->clear();
        ui.action_save_as->setEnabled(false);
        ui.action_save_all->setEnabled(false);
        ui.action_address_book_properties->setEnabled(false);
    }

    if (!hasUnpinnedTabs())
    {
        ui.action_close->setEnabled(false);
        ui.action_close_all->setEnabled(false);
    }
}

void ConsoleWindow::onAddressBookChanged(bool changed)
{
    ui.action_save->setEnabled(changed);

    if (changed)
    {
        ui.action_save_all->setEnabled(true);

        int current_tab_index = ui.tab_widget->currentIndex();
        if (current_tab_index == -1)
            return;

        AddressBookTab* tab =
            dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab_index));
        if (!tab)
            return;

        // Update tab title.
        ui.tab_widget->setTabText(current_tab_index, tab->addressBookName());
    }
}

void ConsoleWindow::onComputerGroupActivated(bool activated, bool is_root)
{
    ui.action_add_computer_group->setEnabled(activated);

    ui.action_add_computer->setEnabled(activated);
    ui.action_modify_computer->setEnabled(false);
    ui.action_delete_computer->setEnabled(false);

    if (is_root)
    {
        ui.action_modify_computer_group->setEnabled(false);
        ui.action_delete_computer_group->setEnabled(false);
    }
    else
    {
        ui.action_modify_computer_group->setEnabled(activated);
        ui.action_delete_computer_group->setEnabled(activated);
    }

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
        if (computer_group)
            ui.status_bar->setCurrentComputerGroup(*computer_group);
        else
            ui.status_bar->clear();
    }
    else
    {
        ui.status_bar->clear();
    }
}

void ConsoleWindow::onComputerActivated(bool activated)
{
    ui.action_modify_computer->setEnabled(activated);
    ui.action_delete_computer->setEnabled(activated);
}

void ConsoleWindow::onComputerGroupContextMenu(const QPoint& point, bool is_root)
{
    QMenu menu;

    if (is_root)
    {
        menu.addAction(ui.action_address_book_properties);
    }
    else
    {
        menu.addAction(ui.action_modify_computer_group);
        menu.addAction(ui.action_delete_computer_group);
    }

    menu.addSeparator();
    menu.addAction(ui.action_add_computer_group);
    menu.addAction(ui.action_add_computer);

    menu.exec(point);
}

void ConsoleWindow::onComputerContextMenu(ComputerItem* computer_item, const QPoint& point)
{
    QMenu menu;

    if (computer_item)
    {
        menu.addAction(ui.action_desktop_manage_connect);
        menu.addAction(ui.action_desktop_view_connect);
        menu.addAction(ui.action_file_transfer_connect);
        menu.addSeparator();
        menu.addAction(ui.action_modify_computer);
        menu.addAction(ui.action_delete_computer);
    }
    else
    {
        menu.addAction(ui.action_add_computer);
    }

    menu.exec(point);
}

void ConsoleWindow::onComputerDoubleClicked(proto::address_book::Computer* computer)
{
    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
        return;

    ComputerItem* computer_item = tab->currentComputer();
    if (!computer_item)
        return;

    if (ui.action_desktop_manage->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);
    }
    else if (ui.action_desktop_view->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_DESKTOP_VIEW);
    }
    else if (ui.action_file_transfer->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_FILE_TRANSFER);
    }
    else
    {
        LOG(LS_FATAL) << "Unknown session type";
        return;
    }

    connectToComputer(*computer);
}

void ConsoleWindow::onTabContextMenu(const QPoint& pos)
{
    QTabBar* tab_bar = ui.tab_widget->tabBar();
    int tab_index = tab_bar->tabAt(pos);
    if (tab_index == -1)
        return;

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(tab_index));
    if (!tab)
        return;

    const QString& current_path = tab->filePath();
    bool is_pinned = mru_.isPinnedFile(current_path);

    QMenu menu;

    QAction* close_other_action = new QAction(
        QIcon(QStringLiteral(":/img/ui-tab-multi-close.png")), tr("Close other tabs"), &menu);
    QAction* close_action;
    QAction* pin_action;

    if (!is_pinned)
    {
        close_action = new QAction(
            QIcon(QStringLiteral(":/img/ui-tab-close.png")), tr("Close tab"), &menu);

        pin_action = new QAction(
            QIcon(QStringLiteral(":/img/lock-unlock.png")), tr("Pin tab"), &menu);
    }
    else
    {
        close_action = nullptr;

        pin_action = new QAction(
            QIcon(QStringLiteral(":/img/lock.png")), tr("Pin tab"), &menu);
    }

    pin_action->setCheckable(true);
    pin_action->setChecked(is_pinned);

    if (close_action)
        menu.addAction(close_action);
    menu.addAction(close_other_action);
    menu.addSeparator();
    menu.addAction(pin_action);

    QAction* action = menu.exec(tab_bar->mapToGlobal(pos));
    if (!action)
        return;

    if (action == close_action)
    {
        onCloseTab(tab_index);
    }
    else if (action == close_other_action)
    {
        for (int i = ui.tab_widget->count(); i >= 0; --i)
        {
            if (i != tab_index)
                onCloseTab(i);
        }
    }
    else if (action == pin_action)
    {
        if (pin_action->isChecked())
        {
            if (!tab->save())
                return;

            ui.tab_widget->setTabIcon(
                tab_index, QIcon(QStringLiteral(":/img/address-book-pinned.png")));
            mru_.pinFile(current_path);
        }
        else
        {
            ui.tab_widget->setTabIcon(
                tab_index, QIcon(QStringLiteral(":/img/address-book.png")));
            mru_.unpinFile(current_path);
        }

        QWidget* close_button = ui.tab_widget->tabBar()->tabButton(tab_index, QTabBar::RightSide);
        close_button->setVisible(!pin_action->isChecked());

        bool has_unpinned_tabs = hasUnpinnedTabs();
        ui.action_close->setEnabled(has_unpinned_tabs);
        ui.action_close_all->setEnabled(has_unpinned_tabs);
    }
}

void ConsoleWindow::onLanguageChanged(QAction* action)
{
    LanguageAction* language_action = dynamic_cast<LanguageAction*>(action);
    if (language_action)
    {
        QString new_locale = language_action->locale();

        locale_loader_.installTranslators(new_locale);
        ui.retranslateUi(this);

        for (int i = 0; i < ui.tab_widget->count(); ++i)
        {
            AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
            if (tab)
                tab->retranslateUi();
        }

        settings_.setLocale(new_locale);
    }
}

void ConsoleWindow::onShowHideToTray()
{
    if (isHidden())
    {
        ui.action_show_hide->setText(tr("Hide"));

        if (!ui.action_show_tray_icon->isChecked())
            showTrayIcon(false);

        if (windowState() & Qt::WindowMaximized)
            showMaximized();
        else
            showNormal();

        activateWindow();
        setFocus();
    }
    else
    {
        ui.action_show_hide->setText(tr("Show"));

        if (!ui.action_show_tray_icon->isChecked())
            showTrayIcon(true);

        hide();
    }
}

void ConsoleWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && ui.action_minimize_to_tray->isChecked())
    {
        if (windowState() & Qt::WindowMinimized)
            onShowHideToTray();
    }

    QMainWindow::changeEvent(event);
}

void ConsoleWindow::closeEvent(QCloseEvent* event)
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
        {
            int ret = QMessageBox(QMessageBox::Question,
                                  tr("Confirmation"),
                                  tr("Address book \"%1\" has been changed. Save changes?")
                                  .arg(tab->addressBookName()),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  this).exec();
            switch (ret)
            {
                case QMessageBox::Yes:
                    tab->save();
                    break;

                case QMessageBox::Cancel:
                    event->ignore();
                    return;

                default:
                    break;
            }
        }
    }

    settings_.setToolBarEnabled(ui.action_toolbar->isChecked());
    settings_.setStatusBarEnabled(ui.action_statusbar->isChecked());
    settings_.setAlwaysShowTrayIcon(ui.action_show_tray_icon->isChecked());
    settings_.setMinimizeToTray(ui.action_minimize_to_tray->isChecked());
    settings_.setWindowGeometry(saveGeometry());
    settings_.setWindowState(saveState());
    settings_.setRecentOpen(mru_.recentOpen());
    settings_.setPinnedFiles(mru_.pinnedFiles());

    if (ui.action_desktop_manage->isChecked())
        settings_.setSessionType(proto::SESSION_TYPE_DESKTOP_MANAGE);
    else if (ui.action_desktop_view->isChecked())
        settings_.setSessionType(proto::SESSION_TYPE_DESKTOP_VIEW);
    else if (ui.action_file_transfer->isChecked())
        settings_.setSessionType(proto::SESSION_TYPE_FILE_TRANSFER);

    QApplication::quit();
    QMainWindow::closeEvent(event);
}

void ConsoleWindow::onUpdateChecked(const updater::UpdateInfo& update_info)
{
    if (!update_info.isValid() || !update_info.hasUpdate())
        return;

    QVersionNumber current_version =
        QVersionNumber(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);

    if (update_info.version() > current_version)
        updater::UpdateDialog(update_info, this).exec();
}

void ConsoleWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : locale_loader_.sortedLocaleList())
    {
        LanguageAction* action_language = new LanguageAction(locale, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
}

void ConsoleWindow::rebuildMruMenu()
{
    ui.menu_recent_open->clear();

    const QStringList file_list = mru_.recentOpen();

    if (file_list.isEmpty())
    {
        QAction* action = new QAction(tr("<empty>"), ui.menu_recent_open);
        action->setEnabled(false);
        ui.menu_recent_open->addAction(action);
    }
    else
    {
        for (const auto& file : file_list)
            ui.menu_recent_open->addAction(new MruAction(file, ui.menu_recent_open));
    }
}

void ConsoleWindow::showTrayIcon(bool show)
{
    if (show)
    {
        tray_menu_.reset(new QMenu(this));
        tray_menu_->addAction(ui.action_show_hide);
        tray_menu_->addAction(ui.action_exit);

        tray_icon_.reset(new QSystemTrayIcon(this));
        tray_icon_->setIcon(QIcon(QStringLiteral(":/img/main.png")));
        tray_icon_->setToolTip(tr("Aspia Console"));
        tray_icon_->setContextMenu(tray_menu_.get());
        tray_icon_->show();

        connect(tray_icon_.get(), &QSystemTrayIcon::activated,
                [this](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::Context)
                return;

            onShowHideToTray();
        });
    }
    else
    {
        tray_icon_.reset();
        tray_menu_.reset();
    }
}

void ConsoleWindow::openAddressBook(const QString& file_path)
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab)
        {
#if defined(OS_WIN)
            if (file_path.compare(tab->filePath(), Qt::CaseInsensitive) == 0)
#else
            if (file_path.compare(tab->addressBookPath(), Qt::CaseSensitive) == 0)
#endif // defined(OS_WIN)
            {
                QMessageBox::information(this,
                                         tr("Information"),
                                         tr("Address Book \"%1\" is already open.").arg(file_path),
                                         QMessageBox::Ok);

                ui.tab_widget->setCurrentIndex(i);
                return;
            }
        }
    }

    AddressBookTab* tab = AddressBookTab::openFromFile(file_path, ui.tab_widget);
    if (!tab)
        return;

    addAddressBookTab(tab);
}

void ConsoleWindow::addAddressBookTab(AddressBookTab* new_tab)
{
    if (!new_tab)
        return;

    const QString& file_path = new_tab->filePath();
    if (mru_.addRecentFile(file_path))
        rebuildMruMenu();

    connect(new_tab, &AddressBookTab::addressBookChanged,
            this, &ConsoleWindow::onAddressBookChanged);
    connect(new_tab, &AddressBookTab::computerGroupActivated,
            this, &ConsoleWindow::onComputerGroupActivated);
    connect(new_tab, &AddressBookTab::computerActivated,
            this, &ConsoleWindow::onComputerActivated);
    connect(new_tab, &AddressBookTab::computerGroupContextMenu,
            this, &ConsoleWindow::onComputerGroupContextMenu);
    connect(new_tab, &AddressBookTab::computerContextMenu,
            this, &ConsoleWindow::onComputerContextMenu);
    connect(new_tab, &AddressBookTab::computerDoubleClicked,
            this, &ConsoleWindow::onComputerDoubleClicked);

    QIcon icon = mru_.isPinnedFile(file_path) ?
        QIcon(QStringLiteral(":/img/address-book-pinned.png")) :
        QIcon(QStringLiteral(":/img/address-book.png"));

    int index = ui.tab_widget->addTab(new_tab, icon, new_tab->addressBookName());

    QWidget* close_button = ui.tab_widget->tabBar()->tabButton(index, QTabBar::RightSide);
    if (mru_.isPinnedFile(file_path))
        close_button->hide();
    else
        close_button->show();

    bool has_unpinned_tabs = hasUnpinnedTabs();
    ui.action_close->setEnabled(has_unpinned_tabs);
    ui.action_close_all->setEnabled(has_unpinned_tabs);

    ui.action_address_book_properties->setEnabled(true);
    ui.action_save_as->setEnabled(true);

    ui.tab_widget->setCurrentIndex(index);
}

AddressBookTab* ConsoleWindow::currentAddressBookTab()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab == -1)
        return nullptr;

    return dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
}

bool ConsoleWindow::hasChangedTabs() const
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
            return true;
    }

    return false;
}

bool ConsoleWindow::hasUnpinnedTabs() const
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && !mru_.isPinnedFile(tab->filePath()))
            return true;
    }

    return false;
}

void ConsoleWindow::connectToComputer(const proto::address_book::Computer& computer)
{
    client::ConnectData connect_data;

    connect_data.computer_name = QString::fromStdString(computer.name());
    connect_data.address       = QString::fromStdString(computer.address());
    connect_data.port          = computer.port();
    connect_data.username      = QString::fromStdString(computer.username());
    connect_data.password      = QString::fromStdString(computer.password());
    connect_data.session_type  = computer.session_type();

    switch (computer.session_type())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            connect_data.desktop_config = computer.session_config().desktop_manage();
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            connect_data.desktop_config = computer.session_config().desktop_view();
            break;

        default:
            break;
    }

    client::ClientWindow::connectToHost(&connect_data);
}

} // namespace console
