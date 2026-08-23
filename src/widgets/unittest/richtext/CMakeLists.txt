find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Test)

add_executable(psirichtext_test
    main.cpp
    ../../psirichtext.cpp
)
target_compile_definitions(psirichtext_test PRIVATE WIDGET_PLUGIN)
target_include_directories(psirichtext_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../..
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/tools
    ${PROJECT_SOURCE_DIR}/src/tools/optionstree
    ${PROJECT_SOURCE_DIR}/iris/include
)
target_link_libraries(psirichtext_test PRIVATE
    Qt${QT_DEFAULT_MAJOR_VERSION}::Test
    ${QT_LIBRARIES}
)

add_test(NAME psirichtext_test COMMAND psirichtext_test)
set_tests_properties(psirichtext_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
