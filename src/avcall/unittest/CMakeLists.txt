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
