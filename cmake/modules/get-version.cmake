cmake_minimum_required(VERSION 3.10.0)

set(VER_FILE ${PROJECT_SOURCE_DIR}/version)
unset(APP_VERSION)
unset(PSI_REVISION)
unset(PSI_PLUS_REVISION)
set(DEFAULT_VER "1.5")

if(NOT Git_FOUND)
  include(FindGit)
  find_package(Git)
  if((NOT IRIS_BUNDLED_USRSCTP OR NOT IRIS_BUNDLED_QCA) AND (NOT Git_FOUND))
    message(STATUS "Git utility not found")
  endif()
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/generate-single-repo.sh")
  set(IS_SNAPSHOT ON)
endif()

function(read_version_file VF_RESULT)
  if(EXISTS "${VER_FILE}")
    message(STATUS "Found Psi version file: ${VER_FILE}")
    file(STRINGS "${VER_FILE}" VER_LINES)
    if(VER_LINES)
      set(${VF_RESULT}
          ${VER_LINES}
          PARENT_SCOPE)
    else()
      message(FATAL_ERROR "Can't read ${VER_FILE} contents")
    endif()
    unset(VER_LINES)
  else()
    message(FATAL_ERROR "${VER_FILE} not found")
  endif()
endfunction()

function(run_git RESULT)
  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} ${ARGN}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE RES
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE EXEC_RESULT
      ERROR_VARIABLE ERROR_1)
    if(EXEC_RESULT EQUAL 0)
      if(NOT RES STREQUAL "")
        set(${RESULT}
            "${RES}"
            PARENT_SCOPE)
      endif()
    else()
      string(JOIN " " GIT_ARGS ${ARGN})
      message(WARNING "Can't execute ${GIT_EXECUTABLE} ${GIT_ARGS}: ${ERROR_1}")
    endif()
  endif()
endfunction()

function(obtain_git_version GIT_VERSION GIT_FULL_VERSION)
  if(EXISTS "${PROJECT_SOURCE_DIR}/\.git" AND (NOT IS_SNAPSHOT))
    run_git(MAIN_VER describe --tags --abbrev=0 --exclude "windows-sdk-*")
    if(MAIN_VER)
      set(APP_VERSION
          "${MAIN_VER}"
          PARENT_SCOPE)
      set(${GIT_VERSION}
          ${MAIN_VER}
          PARENT_SCOPE)
      run_git(COMMITS rev-list --count "${MAIN_VER}\.\.HEAD")
      if(NOT COMMITS STREQUAL "")
        run_git(VER_HASH rev-parse --short HEAD)
        if(NOT VER_HASH STREQUAL "")
          set(PSI_REVISION
              ${VER_HASH}
              PARENT_SCOPE)
          set(APP_VERSION
              "${MAIN_VER}.${COMMITS}"
              PARENT_SCOPE)
          set(${GIT_FULL_VERSION}
              "${MAIN_VER}.${COMMITS} \(${PSI_COMPILATION_DATE}, ${VER_HASH}${PSI_VER_SUFFIX}\)"
              PARENT_SCOPE)
        endif()
      endif()
    endif()
  elseif(IS_SNAPSHOT)
    message(
      STATUS "${PROJECT_NAME} snapshot detected. Reading version from file...")
  else()
    message(
      STATUS
        "${PROJECT_SOURCE_DIR}/\.git directory not found. Reading version from file..."
    )
  endif()
endfunction()

function(obtain_psi_file_version VERSION_LINES OUTPUT_VERSION)
  # match version in formats N.N.N and N.N_SN.N and even N.N_SN.N_SN
  # where N - number and S - string (aA or bB)
  string(REGEX MATCH "^([0-9]+(\\.[0-9a-bA-B_]+)+)+" VER_LINE_A "${VERSION_LINES}")
  set(_APP_VERSION "${CMAKE_MATCH_1}")
  # specifies the revision, if it is present within the parentheses
  if("${VERSION_LINES}" MATCHES "\\(([^)]*)\\)")
    set(_INNER_TEXT "${CMAKE_MATCH_1}")
    if("${_INNER_TEXT}" MATCHES ", ([a-fA-F0-9]+)")
      set(_PSI_REVISION "${CMAKE_MATCH_1}")
    endif()
  endif()
  if(_APP_VERSION)
    message(STATUS "Psi version found: ${_APP_VERSION}")
  else()
    message(
      WARNING "Psi+ version not found! ${DEFAULT_VER} version will be set")
      set(_APP_VERSION ${DEFAULT_VER})
  endif()
  set(APP_VERSION ${_APP_VERSION} PARENT_SCOPE)
  # Psi revision
  if(_PSI_REVISION)
    set(PSI_REVISION ${_PSI_REVISION} PARENT_SCOPE)
    message(STATUS "Psi revision found: ${_PSI_REVISION}")
  endif()
  # Form OUTPUT_VERSION
  if(_APP_VERSION AND _PSI_REVISION)
    set(${OUTPUT_VERSION}
    "${_APP_VERSION} \(${PSI_COMPILATION_DATE}, ${_PSI_REVISION}${PSI_VER_SUFFIX}\)"
    PARENT_SCOPE)
  else()
    if(PRODUCTION AND (NOT PSI_PLUS))
      set(${OUTPUT_VERSION} "${_APP_VERSION}" PARENT_SCOPE)
    else()
      set(${OUTPUT_VERSION}
      "${_APP_VERSION} \(${PSI_COMPILATION_DATE}${PSI_VER_SUFFIX}\)"
      PARENT_SCOPE)
    endif()
  endif()
endfunction()

if(NOT PSI_VERSION)
  obtain_git_version(PSIVER GITVER)
  if(PSIVER AND GITVER)
    message(STATUS "Git Version ${PSIVER}")
    if(PRODUCTION AND (NOT PSI_PLUS))
      set(PSI_VERSION "${PSIVER}")
    else()
      set(PSI_VERSION "${GITVER}")
    endif()
  else()
    read_version_file(VER_LINES)
    obtain_psi_file_version("${VER_LINES}" FILEVER)
    if(FILEVER)
      set(PSI_VERSION "${FILEVER}")
    endif()
  endif()
  unset(VER_LINES)
endif()

# An explicitly supplied PSI_VERSION bypasses obtain_git_version()/read_version_file(),
# leaving APP_VERSION unset. Qt's Windows resource generator needs a strictly
# numeric version independent from the user-facing version string.
# E.g. show 1.6_b1 in Psi but embed 1.6.1.0 in the Windows PE resources.
if(NOT APP_VERSION AND PSI_VERSION)
  if(PSI_VERSION MATCHES "^([0-9]+)\\.([0-9]+)_b([0-9]+)$")
    set(APP_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}.0")
  elseif(PSI_VERSION MATCHES "^([0-9]+(\\.[0-9]+)+)")
    set(APP_VERSION "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR "Cannot derive numeric APP_VERSION from PSI_VERSION='${PSI_VERSION}'")
  endif()
endif()

message(STATUS "${CLIENT_NAME} version set: ${PSI_VERSION}")
message(STATUS "APP_VERSION: ${APP_VERSION}")
