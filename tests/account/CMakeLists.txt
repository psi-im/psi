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

add_executable(account_avatar_blob_inflight_test avatarblobinflighttest.cpp)
set_target_properties(account_avatar_blob_inflight_test PROPERTIES AUTOMOC ON)
target_include_directories(account_avatar_blob_inflight_test PRIVATE ../../src)
target_link_libraries(account_avatar_blob_inflight_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME account_avatar_blob_inflight COMMAND account_avatar_blob_inflight_test)
set_tests_properties(account_avatar_blob_inflight PROPERTIES TIMEOUT 10)

add_executable(account_avatar_negative_policy_test avatarnegativepolicytest.cpp)
set_target_properties(account_avatar_negative_policy_test PROPERTIES AUTOMOC ON)
target_include_directories(account_avatar_negative_policy_test PRIVATE ../../src)
target_link_libraries(account_avatar_negative_policy_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
add_test(NAME account_avatar_negative_policy COMMAND account_avatar_negative_policy_test)
set_tests_properties(account_avatar_negative_policy PROPERTIES TIMEOUT 10)

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

    # Bundled static Iris archives reference each other (irisnet's DTLS uses
    # XMPP::Hash). A consumer needing only Jid pulls too little from libiris on
    # the first pass, so list libiris again after libirisnet.
    set(ACCOUNT_IRIS_STATIC_CYCLE)
    if(TARGET iris AND TARGET irisnet)
        set(ACCOUNT_IRIS_STATIC_CYCLE $<TARGET_FILE:irisnet> $<TARGET_FILE:iris>)
    endif()

    if(IRIS_ENABLE_OMEMO)
        find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Sql)
        add_executable(account_omemo_storage_test omemostoragetest.cpp
            ../../src/omemodevicedecision.cpp ../../src/psiomemostorage.cpp)
        target_compile_definitions(account_omemo_storage_test PRIVATE IRIS_ENABLE_OMEMO)
        set_target_properties(account_omemo_storage_test PROPERTIES AUTOMOC ON)
        target_include_directories(account_omemo_storage_test PRIVATE ../../src)
        target_link_libraries(account_omemo_storage_test PRIVATE ${ACCOUNT_IRIS_TARGET}
            ${ACCOUNT_IRIS_STATIC_CYCLE} Qt${QT_DEFAULT_MAJOR_VERSION}::Sql Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
        add_test(NAME account_omemo_storage COMMAND account_omemo_storage_test)
        set_tests_properties(account_omemo_storage PROPERTIES TIMEOUT 15)

        # Reuse Iris's network-free PEP fixture when its sources are available.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../iris/tests/jingle/omemoelement.cpp")
            add_executable(account_omemo_exchange_test omemostorageexchange.cpp ../../src/psiomemostorage.cpp)
            target_compile_definitions(account_omemo_exchange_test PRIVATE IRIS_ENABLE_OMEMO)
            target_include_directories(account_omemo_exchange_test PRIVATE ../../src)
            target_link_libraries(account_omemo_exchange_test PRIVATE ${ACCOUNT_IRIS_TARGET}
                ${ACCOUNT_IRIS_STATIC_CYCLE} Qt${QT_DEFAULT_MAJOR_VERSION}::Sql)
            add_test(NAME account_omemo_exchange COMMAND account_omemo_exchange_test)
            set_tests_properties(account_omemo_exchange PROPERTIES TIMEOUT 30)
        endif()
    endif()

    # Disk-cache reader uses real Iris modern and legacy vCard parsers.
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Xml)
    add_executable(account_vcard_disk_cache_test vcarddiskcachetest.cpp ../../src/vcardfactory_data.cpp)
    set_target_properties(account_vcard_disk_cache_test PROPERTIES AUTOMOC ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(account_vcard_disk_cache_test PRIVATE ../../src)
    target_link_libraries(account_vcard_disk_cache_test PRIVATE ${ACCOUNT_IRIS_TARGET}
        ${ACCOUNT_IRIS_STATIC_CYCLE} Qt${QT_DEFAULT_MAJOR_VERSION}::Xml Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
    add_test(NAME account_vcard_disk_cache COMMAND account_vcard_disk_cache_test)
    set_tests_properties(account_vcard_disk_cache PROPERTIES TIMEOUT 15)

    add_executable(account_vcard_request_merge_test vcardrequestmergetest.cpp ../../src/vcardfactory_data.cpp)
    set_target_properties(account_vcard_request_merge_test PROPERTIES AUTOMOC ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(account_vcard_request_merge_test PRIVATE ../../src)
    target_link_libraries(account_vcard_request_merge_test PRIVATE ${ACCOUNT_IRIS_TARGET}
        ${ACCOUNT_IRIS_STATIC_CYCLE} Qt${QT_DEFAULT_MAJOR_VERSION}::Xml Qt${QT_DEFAULT_MAJOR_VERSION}::Test)
    add_test(NAME account_vcard_request_merge COMMAND account_vcard_request_merge_test)
    set_tests_properties(account_vcard_request_merge PROPERTIES TIMEOUT 15)

endif()
