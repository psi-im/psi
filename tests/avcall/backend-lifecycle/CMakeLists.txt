find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Test)

add_executable(avcall_backend_lifecycle_test
    tst_backend_lifecycle.cpp
)
target_include_directories(avcall_backend_lifecycle_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src/avcall
    ${PROJECT_SOURCE_DIR}/src/psimedia
    ${PROJECT_SOURCE_DIR}/plugins/include
    ${PROJECT_SOURCE_DIR}/iris/include/iris
    ${PROJECT_SOURCE_DIR}/iris/src
)
target_link_libraries(avcall_backend_lifecycle_test PRIVATE
    avcall
    psimedia
    Qt${QT_DEFAULT_MAJOR_VERSION}::Test
    ${QT_LIBRARIES}
    ${iris_LIB}
)

add_test(NAME avcall_backend_lifecycle_test COMMAND avcall_backend_lifecycle_test)
set_tests_properties(avcall_backend_lifecycle_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(avcall_capability_refresh_test
    tst_capability_refresh.cpp
)
target_include_directories(avcall_capability_refresh_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src/avcall
    ${PROJECT_SOURCE_DIR}/src/psimedia
    ${PROJECT_SOURCE_DIR}/plugins/include
    ${PROJECT_SOURCE_DIR}/iris/include/iris
    ${PROJECT_SOURCE_DIR}/iris/src
)
target_link_libraries(avcall_capability_refresh_test PRIVATE
    avcall
    psimedia
    Qt${QT_DEFAULT_MAJOR_VERSION}::Test
    ${QT_LIBRARIES}
    ${iris_LIB}
)

add_test(NAME avcall_capability_refresh_test COMMAND avcall_capability_refresh_test)
set_tests_properties(avcall_capability_refresh_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
