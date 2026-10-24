cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(psi_account_regressions LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major version")
    enable_testing()
endif()
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core Test)
add_executable(account_connection_guard_test connectionguard.cpp)
set_target_properties(account_connection_guard_test PROPERTIES AUTOMOC ON)
target_include_directories(account_connection_guard_test PRIVATE ../../src)
target_link_libraries(account_connection_guard_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME account_connection_guard COMMAND account_connection_guard_test)
set_tests_properties(account_connection_guard PROPERTIES TIMEOUT 10)

if(TARGET iris)
    set(ACCOUNT_IRIS_TARGET iris)
elseif(TARGET Iris::Iris)
    set(ACCOUNT_IRIS_TARGET Iris::Iris)
endif()
if(ACCOUNT_IRIS_TARGET)
    add_executable(account_message_receipt_test messagereceipt.cpp ../../src/accountincomingmessage.cpp)
    set_target_properties(account_message_receipt_test PROPERTIES AUTOMOC ON)
    target_include_directories(account_message_receipt_test PRIVATE ../../src)
    target_link_libraries(account_message_receipt_test PRIVATE ${ACCOUNT_IRIS_TARGET}
        Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
    add_test(NAME account_message_receipt COMMAND account_message_receipt_test)
    set_tests_properties(account_message_receipt PROPERTIES TIMEOUT 15)

    # The real NativeChatAvatars with PsiAccount/AvatarFactory/MessageView doubles.
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Gui)
    # Compile a copy outside src/, so its quoted includes resolve to the doubles.
    configure_file(../../src/nativechatavatars.cpp ${CMAKE_CURRENT_BINARY_DIR}/nativechatavatars_impl.cpp COPYONLY)
    add_executable(account_native_chat_avatars_test nativechatavatars.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/nativechatavatars_impl.cpp)
    set_target_properties(account_native_chat_avatars_test PROPERTIES AUTOMOC ON)
    target_include_directories(account_native_chat_avatars_test BEFORE PRIVATE nativechatavatars ../../src)
    # Bundled static Iris archives reference each other (irisnet's DTLS uses
    # XMPP::Hash). A consumer needing only Jid pulls too little from libiris on
    # the first pass, so list libiris again after libirisnet.
    set(ACCOUNT_IRIS_STATIC_CYCLE)
    if(TARGET iris AND TARGET irisnet)
        set(ACCOUNT_IRIS_STATIC_CYCLE $<TARGET_FILE:irisnet> $<TARGET_FILE:iris>)
    endif()
    # Production vCard XML reader with the *real* Iris vCard4/temp parsers.
    # Iris's public vCard4 headers require C++20 (`std::ranges`).
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Xml)
    add_executable(account_vcard_disk_cache_test vcarddiskcachetest.cpp)
    set_target_properties(account_vcard_disk_cache_test PROPERTIES
        AUTOMOC ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(account_vcard_disk_cache_test PRIVATE ../../src)
    target_link_libraries(account_vcard_disk_cache_test PRIVATE ${ACCOUNT_IRIS_TARGET}
        ${ACCOUNT_IRIS_STATIC_CYCLE}
        Qt${QT_DEFAULT_MAJOR_VERSION}::Core
        Qt${QT_DEFAULT_MAJOR_VERSION}::Xml
        Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
    add_test(NAME account_vcard_disk_cache COMMAND account_vcard_disk_cache_test)
    set_tests_properties(account_vcard_disk_cache PROPERTIES TIMEOUT 15)

    target_link_libraries(account_native_chat_avatars_test PRIVATE ${ACCOUNT_IRIS_TARGET}
        ${ACCOUNT_IRIS_STATIC_CYCLE} Qt${QT_DEFAULT_MAJOR_VERSION}::Gui Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
    add_test(NAME account_native_chat_avatars COMMAND account_native_chat_avatars_test)
    set_tests_properties(account_native_chat_avatars PROPERTIES TIMEOUT 10 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
