cmake_minimum_required(VERSION 3.16)
project(psi_mam_native_widgets LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
enable_testing()
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major version")
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core Widgets Test Multimedia Network)
set(PSI_WIDGETS ${CMAKE_CURRENT_LIST_DIR}/../../src/widgets)
if(NOT TARGET qsourcehighlite)
    include(${CMAKE_CURRENT_LIST_DIR}/../../3rdparty/qsourcehighlite.cmake)
endif()
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

# Tests the actual PsiApplication event bridge, including Qt's theme-change
# ordering and coalesced notification to palette-dependent widgets.
# macOS PsiApplication additionally links CocoaTrayClick; the existing widget
# regression CI runs this Qt-only fixture on Linux.
if(NOT APPLE)
    add_executable(mam_psi_application_palette_test PsiApplicationPaletteTest.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../src/psiapplication.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../src/psiapplication.h)
    target_include_directories(mam_psi_application_palette_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../src)
    target_link_libraries(mam_psi_application_palette_test PRIVATE
        Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
    add_test(NAME mam_psi_application_palette COMMAND mam_psi_application_palette_test)
endif()

add_executable(mam_thin_splitter_test ThinSplitterTest.cpp
    ${PSI_WIDGETS}/ThinSplitter.cpp ${PSI_WIDGETS}/ThinSplitter.h)
target_include_directories(mam_thin_splitter_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_thin_splitter_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_thin_splitter COMMAND mam_thin_splitter_test)

add_executable(mam_identity_test MamIdentityTest.cpp)
target_include_directories(mam_identity_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_identity_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_identity COMMAND mam_identity_test)

# OOB/SIMS media classification is shared by live and MAM message rendering.
add_executable(mam_oob_media_policy_test ../filesharing/oobmedia.cpp)
target_include_directories(mam_oob_media_policy_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_oob_media_policy_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_oob_media_policy COMMAND mam_oob_media_policy_test)

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
    ${PSI_WIDGETS}/NativeChatRenderers.cpp ${PSI_WIDGETS}/NativeChatRenderers.h
    ${PSI_WIDGETS}/MamImageViewer.cpp ${PSI_WIDGETS}/MamImageViewer.h
    ${PSI_WIDGETS}/MamHistorySource.cpp ${PSI_WIDGETS}/MamHistorySource.h
    ${PSI_WIDGETS}/ChatMessageText.cpp ${PSI_WIDGETS}/ChatMessageText.h
    ${PSI_WIDGETS}/CodeBlockWidget.cpp ${PSI_WIDGETS}/CodeBlockWidget.h
    ${PSI_WIDGETS}/OverlayScrollBarManager.cpp ${PSI_WIDGETS}/OverlayScrollBarManager.h
    ${PSI_WIDGETS}/MessagePreview.cpp ${PSI_WIDGETS}/MessagePreview.h
    ${PSI_WIDGETS}/LinkButton.cpp ${PSI_WIDGETS}/LinkButton.h
    ${PSI_WIDGETS}/psirichtext.cpp
    ${PSI_WIDGETS}/psicodehighlight.cpp
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
target_link_libraries(mam_chat_widget_test PRIVATE qsourcehighlite
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test
    Qt${QT_DEFAULT_MAJOR_VERSION}::Multimedia Qt${QT_DEFAULT_MAJOR_VERSION}::Network)
add_test(NAME mam_chat_widget COMMAND mam_chat_widget_test)

add_executable(mam_image_viewer_test MamImageViewerTest.cpp
    ${PSI_WIDGETS}/MamImageViewer.cpp ${PSI_WIDGETS}/MamImageViewer.h)
target_include_directories(mam_image_viewer_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_image_viewer_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Widgets Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_image_viewer COMMAND mam_image_viewer_test)

add_executable(mam_native_theme_test NativeChatThemeTest.cpp
    ${PSI_WIDGETS}/NativeChatThemeRegistry.cpp ${PSI_WIDGETS}/NativeChatThemeRegistry.h
    ${PSI_WIDGETS}/NativeChatRenderers.cpp ${PSI_WIDGETS}/NativeChatRenderers.h)
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

# The XEP-0393 parser is intentionally Qt-only and shared by all Psi views.
add_executable(mam_message_styling_test MessageStylingTest.cpp)
target_include_directories(mam_message_styling_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(mam_message_styling_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME mam_message_styling COMMAND mam_message_styling_test)
