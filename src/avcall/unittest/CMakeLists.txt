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

add_executable(avcallaudiodirection_test
    audiodirection.cpp
    ../avcallaudiodirection.cpp
    ../avcallaudiodirection.h
)
target_include_directories(avcallaudiodirection_test PRIVATE ${PROJECT_SOURCE_DIR}/iris/include)
target_link_libraries(avcallaudiodirection_test PRIVATE ${iris_LIB} Qt${QT_DEFAULT_MAJOR_VERSION}::Core)
add_test(NAME avcallaudiodirection_test COMMAND avcallaudiodirection_test)
set_tests_properties(avcallaudiodirection_test PROPERTIES TIMEOUT 10)
