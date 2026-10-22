# Regression: an explicitly requested beta must have Windows-compatible numeric
# resource metadata without losing its user-facing version string.
set(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
set(CLIENT_NAME "Psi")
set(PSI_VERSION "1.6_b1")
include("${PROJECT_SOURCE_DIR}/cmake/modules/get-version.cmake")
if(NOT APP_VERSION STREQUAL "1.6.1.0")
  message(FATAL_ERROR "Expected APP_VERSION=1.6.1.0, got '${APP_VERSION}'")
endif()
if(NOT PSI_VERSION STREQUAL "1.6_b1")
  message(FATAL_ERROR "Expected PSI_VERSION=1.6_b1, got '${PSI_VERSION}'")
endif()
message(STATUS "Psi beta version metadata validated")
