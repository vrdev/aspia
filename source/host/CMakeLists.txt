#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

include(translations)

list(APPEND SOURCE_HOST
    desktop_config_tracker.cc
    desktop_config_tracker.h
    host.rc
    host_config_main.cc
    host_config_main.h
    host_export.h
    host_notifier.cc
    host_notifier.h
    host_server.cc
    host_server.h
    host_session.cc
    host_session.h
    host_session_desktop.cc
    host_session_desktop.h
    host_session_fake.cc
    host_session_fake.h
    host_session_fake_desktop.cc
    host_session_fake_desktop.h
    host_session_fake_file_transfer.cc
    host_session_fake_file_transfer.h
    host_session_file_transfer.cc
    host_session_file_transfer.h
    host_settings.cc
    host_settings.h
    host_system_info.cc
    host_system_info.h
    input_injector.cc
    input_injector.h
    screen_updater.cc
    screen_updater.h
    screen_updater_impl.cc
    screen_updater_impl.h)

list(APPEND SOURCE_HOST_RESOURCES
    resources/host.qrc)

list(APPEND SOURCE_HOST_UI
    ui/host_config_dialog.cc
    ui/host_config_dialog.h
    ui/host_config_dialog.ui
    ui/host_notifier_window.cc
    ui/host_notifier_window.h
    ui/host_notifier_window.ui
    ui/user_dialog.cc
    ui/user_dialog.h
    ui/user_dialog.ui
    ui/user_tree_item.cc
    ui/user_tree_item.h)

list(APPEND SOURCE_HOST_WIN
    win/dll_main.cc
    win/host.cc
    win/host.h
    win/host_main.cc
    win/host_main.h
    win/host_process.cc
    win/host_process.h
    win/host_process_impl.cc
    win/host_process_impl.h
    win/host_service.cc
    win/host_service.h
    win/host_service_constants.cc
    win/host_service_constants.h
    win/host_service_main.cc
    win/host_service_main.h
    win/updater_launcher.cc
    win/updater_launcher.h)

source_group("" FILES ${SOURCE_HOST})
source_group(moc FILES ${SOURCE_HOST_MOC})
source_group(resources FILES ${SOURCE_HOST_RESOURCES})
source_group(ui FILES ${SOURCE_HOST_UI})
source_group(win FILES ${SOURCE_HOST_WIN})

add_library(aspia_host SHARED
    ${SOURCE_HOST}
    ${SOURCE_HOST_RESOURCES}
    ${SOURCE_HOST_UI}
    ${SOURCE_HOST_WIN})
target_link_libraries(aspia_host
    aspia_base
    aspia_codec
    aspia_common
    aspia_crypto
    aspia_desktop
    aspia_ipc
    aspia_net
    aspia_proto
    aspia_updater
    ${THIRD_PARTY_LIBS})

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB HOST_TS_FILES translations/*.ts)
    add_translations(aspia_host QRC_FILE "translations/host_translations.qrc" TS_FILES ${HOST_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE HOST_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/host/*.cc
        ${PROJECT_SOURCE_DIR}/host/*.cpp
        ${PROJECT_SOURCE_DIR}/host/*.h
        ${PROJECT_SOURCE_DIR}/host/*.ui)

    qt5_create_translation(TS_HOST_UPDATE ${HOST_TR_SOURCES} ${HOST_TS_FILES})
    add_custom_target(aspia_host_ts_update DEPENDS ${TS_HOST_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()

add_executable(aspia_host_config
    host_config_entry_point.cc
    host_config.rc)
set_target_properties(aspia_host_config PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_config PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_config aspia_host)

add_executable(aspia_host_service
    win/host_service_entry_point.cc
    win/host_service.rc)
set_target_properties(aspia_host_service PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_service aspia_host)

add_executable(aspia_host_session
    win/host_session_entry_point.cc
    win/host_session.rc)
set_target_properties(aspia_host_session PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_session PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_session aspia_host)
