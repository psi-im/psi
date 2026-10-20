cmake_minimum_required(VERSION 3.16)
project(psi_mam_native_widgets LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
enable_testing()
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major version")
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core Widgets Test Multimedia Network)
set(PSI_WIDGETS ${CMAKE_CURRENT_LIST_DIR}/../../src/widgets)
add_executable(mam_long_list_test LongListWidgetTest.cpp
    ${PSI_WIDGETS}/LongListWidget.cpp ${PSI_WIDGETS}/LongListWidgetLayout.cpp ${PSI_WIDGETS}/LongListWidget.h)
target_include_directories(mam_long_list_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_long_list_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_long_list COMMAND mam_long_list_test)

add_executable(mam_overlay_scrollbar_test OverlayScrollBarManagerTest.cpp
    ${PSI_WIDGETS}/OverlayScrollBarManager.cpp ${PSI_WIDGETS}/OverlayScrollBarManager.h)
target_include_directories(mam_overlay_scrollbar_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_overlay_scrollbar_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_overlay_scrollbar COMMAND mam_overlay_scrollbar_test)

add_executable(mam_identity_test MamIdentityTest.cpp)
target_include_directories(mam_identity_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_identity_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_identity COMMAND mam_identity_test)

add_executable(mam_history_source_policy_test MamHistorySourcePolicyTest.cpp)
target_include_directories(mam_history_source_policy_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_history_source_policy_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_history_source_policy COMMAND mam_history_source_policy_test)

add_executable(mam_archive_peer_policy_test MamArchivePeerPolicyTest.cpp)
target_include_directories(mam_archive_peer_policy_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_archive_peer_policy_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_archive_peer_policy COMMAND mam_archive_peer_policy_test)

# Actual MamChatWidget/LongListWidget implementation, backed by a small
# MessageView test double so no PsiAccount or application singleton is needed.
# PsiTextView, PsiRichText and qite are real, including media parsers/players.
# Test doubles supply deterministic icon assets and the account/storage boundary.
add_executable(mam_chat_widget_test MamChatWidgetTest.cpp
    ${PSI_WIDGETS}/MamChatWidget.cpp ${PSI_WIDGETS}/MamChatWidget.h
    ${PSI_WIDGETS}/MamHistorySource.cpp ${PSI_WIDGETS}/MamHistorySource.h
    ${PSI_WIDGETS}/ChatMessageText.cpp ${PSI_WIDGETS}/ChatMessageText.h
    ${PSI_WIDGETS}/psirichtext.cpp
    ${PSI_WIDGETS}/psitextview.cpp ${PSI_WIDGETS}/psitextview.h
    ${PSI_WIDGETS}/urlobject.h mock/urlobject.cpp
    mock/filesharingmanager.h mock/filesharingdownloader.h
    ../../3rdparty/qite/libqite/qite.cpp
    ../../3rdparty/qite/libqite/qitemedia.cpp
    ../../3rdparty/qite/libqite/qiteaudio.cpp
    ../../3rdparty/qite/libqite/qitevideo.cpp
    ../../3rdparty/qite/libqite/qitevideoframe.cpp
    ${PSI_WIDGETS}/LongListWidget.cpp ${PSI_WIDGETS}/LongListWidgetLayout.cpp ${PSI_WIDGETS}/LongListWidget.h)
target_include_directories(mam_chat_widget_test BEFORE PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/mock
    ${CMAKE_CURRENT_LIST_DIR}/../../src
    ${CMAKE_CURRENT_LIST_DIR}/../../3rdparty/qite/libqite)
target_link_libraries(mam_chat_widget_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test
    Qt${QT_DEFAULT_MAJOR_VERSION}::Multimedia Qt${QT_DEFAULT_MAJOR_VERSION}::Network)
add_test(NAME mam_chat_widget COMMAND mam_chat_widget_test)

add_executable(mam_native_theme_test NativeChatThemeTest.cpp
    ${PSI_WIDGETS}/NativeChatThemeRegistry.cpp ${PSI_WIDGETS}/NativeChatThemeRegistry.h)
target_include_directories(mam_native_theme_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_native_theme_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_native_theme COMMAND mam_native_theme_test)

add_executable(mam_sparse_history_source_test MamSparseHistorySourceTest.cpp
    ${PSI_WIDGETS}/MamHistorySource.cpp ${PSI_WIDGETS}/MamHistorySource.h)
target_include_directories(mam_sparse_history_source_test BEFORE PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/mock ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_sparse_history_source_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_sparse_history_source COMMAND mam_sparse_history_source_test)
