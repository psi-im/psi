# QSourceHighlite (MIT), vendored from upstream d78f0d4171580a18646b15425c449db3f2bb240a
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Gui)
add_library(qsourcehighlite STATIC
    qsourcehighliter.cpp
    qsourcehighliterthemes.cpp
    languagedata.cpp
)
target_include_directories(qsourcehighlite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qsourcehighlite PUBLIC Qt${QT_DEFAULT_MAJOR_VERSION}::Gui)
if(MSVC)
    target_compile_options(qsourcehighlite PRIVATE /WX-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qsourcehighlite PRIVATE -Wno-error)
endif()
