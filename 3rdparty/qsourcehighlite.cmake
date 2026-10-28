# Build our pinned upstream QSourceHighlite git submodule without modifying it.
# Shared by the Psi application and the standalone MAM widget test project.
set(_psi_source_highlite_dir "${CMAKE_CURRENT_LIST_DIR}/qsourcehighlite")
if(NOT EXISTS "${_psi_source_highlite_dir}/qsourcehighliter.cpp")
    message(FATAL_ERROR "QSourceHighlite submodule missing: run git submodule update --init 3rdparty/qsourcehighlite")
endif()
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Gui)
add_library(qsourcehighlite STATIC
    "${_psi_source_highlite_dir}/qsourcehighliter.cpp"
    "${_psi_source_highlite_dir}/qsourcehighliterthemes.cpp"
    "${_psi_source_highlite_dir}/languagedata.cpp"
)
target_include_directories(qsourcehighlite PUBLIC "${_psi_source_highlite_dir}")
target_link_libraries(qsourcehighlite PUBLIC Qt${QT_DEFAULT_MAJOR_VERSION}::Gui)
# Vendor diagnostics are visible, but not promoted to errors by Psi's strict flags.
if(MSVC)
    target_compile_options(qsourcehighlite PRIVATE /WX-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qsourcehighlite PRIVATE -Wno-error)
endif()
