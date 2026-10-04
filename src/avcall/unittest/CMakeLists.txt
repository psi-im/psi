add_executable(avcallpolicy_test
    policy.cpp
)

target_include_directories(avcallpolicy_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${PROJECT_SOURCE_DIR}/iris/include
)

target_link_libraries(avcallpolicy_test PRIVATE
    ${iris_LIB}
    Qt${QT_DEFAULT_MAJOR_VERSION}::Core
)

add_test(NAME avcallpolicy_test COMMAND avcallpolicy_test)

add_executable(avcallincomingcontent_test incomingcontent.cpp)
target_include_directories(avcallincomingcontent_test PRIVATE ${PROJECT_SOURCE_DIR}/iris/include)
target_link_libraries(avcallincomingcontent_test PRIVATE ${iris_LIB} Qt${QT_DEFAULT_MAJOR_VERSION}::Core)
add_test(NAME avcallincomingcontent_test COMMAND avcallincomingcontent_test)
set_tests_properties(avcallincomingcontent_test PROPERTIES TIMEOUT 10)

add_executable(avcallaudiodirection_test
    audiodirection.cpp
    ../avcallaudiodirection.cpp
    ../avcallaudiodirection.h
)
target_include_directories(avcallaudiodirection_test PRIVATE ${PROJECT_SOURCE_DIR}/iris/include)
target_link_libraries(avcallaudiodirection_test PRIVATE ${iris_LIB} Qt${QT_DEFAULT_MAJOR_VERSION}::Core)
add_test(NAME avcallaudiodirection_test COMMAND avcallaudiodirection_test)
set_tests_properties(avcallaudiodirection_test PROPERTIES TIMEOUT 10)

if(LINUX AND USE_DBUS)
    find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Test)
    find_program(DBUS_RUN_SESSION dbus-run-session)
    if(DBUS_RUN_SESSION)
        add_executable(avcall_screenshare_test screenshare.cpp ../screensharecapture.cpp ../screensharecapture.h
                       ../callview.cpp ../callview.h)
        target_include_directories(avcall_screenshare_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
        target_compile_definitions(avcall_screenshare_test PRIVATE PSI_SCREENSHARE_PORTAL)
        target_link_libraries(avcall_screenshare_test PRIVATE psimedia Qt${QT_DEFAULT_MAJOR_VERSION}::Test
                              ${QT_LIBRARIES})
        add_test(NAME avcall_screenshare_test COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:avcall_screenshare_test>)
        set_tests_properties(avcall_screenshare_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
    endif()
endif()
