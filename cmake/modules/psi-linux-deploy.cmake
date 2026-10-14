include(GNUInstallDirs)

# Templates relative to the plugins directory.
# Only the Qt plugins required by the application are listed here.
set(DEPLOY_QT_PLUGIN_PATTERNS
    "imageformats/libqgif.so"
    "imageformats/libqheif.so"
    "imageformats/libqicns.so"
    "imageformats/libqico.so"
    "imageformats/libqjpeg.so"
    "imageformats/libqmng.so"
    "imageformats/libqpdf.so"
    "imageformats/libqsvg.so"
    "imageformats/libqtga.so"
    "imageformats/libqtiff.so"
    "imageformats/libqwbmp.so"
    "imageformats/libqwebp.so"
    "iconengines/libqsvgicon.so"
    "multimedia/libffmpegmediaplugin.so"
    "networkinformation/libqnetworkmanager.so"
    "platforminputcontexts/libcomposeplatforminputcontextplugin.so"
    "platforms/libqwayland-generic.so"
    "platforms/libqxcb.so"
    "platformthemes/libqxdgdesktopportal.so"
    "position/libqtposition_nmea.so"
    "position/libqtposition_positionpoll.so"
    "sqldrivers/libqsqlite.so"
    "tls/libqcertonlybackend.so"
    "tls/libqopensslbackend.so"
    "xcbglintegrations/libqxcb-glx-integration.so"
    "wayland-decoration-client/libbradient.so"
    "wayland-shell-integration/libfullscreen-shell-v1.so"
    "wayland-shell-integration/libivi-shell.so"
    "wayland-shell-integration/libqt-shell.so"
    "wayland-shell-integration/libwl-shell-plugin.so"
    "wayland-shell-integration/libxdg-shell.so"
    "/usr/${CMAKE_INSTALL_LIBDIR}/qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto/*.so"
    "/usr/lib/qca3-qt${QT_DEFAULT_MAJOR_VERSION}/crypto/*.so"
)

# GStreamer plugin masks, if they are to be bundled with the AppImage.
# For example, with the standard GStreamer layout on Linux:
set(DEPLOY_GSTREAMER_PLUGIN_PATTERNS
    "gstreamer-1.0/libgstalsa.so"
    "gstreamer-1.0/libgstapp.so"
    "gstreamer-1.0/libgstaudioconvert.so"
    "gstreamer-1.0/libgstaudiomixer.so"
    "gstreamer-1.0/libgstaudioresample.so"
    "gstreamer-1.0/libgstcoreelements.so"
    "gstreamer-1.0/libgstdecklink.so"
    "gstreamer-1.0/libgstjpeg.so"
    "gstreamer-1.0/libgstlevel.so"
    "gstreamer-1.0/libgstogg.so"
    "gstreamer-1.0/libgstopus.so"
    "gstreamer-1.0/libgstossaudio.so"
    "gstreamer-1.0/libgstpipewire.so"
    "gstreamer-1.0/libgstplayback.so"
    "gstreamer-1.0/libgstpulseaudio.so"
    "gstreamer-1.0/libgstrtp.so"
    "gstreamer-1.0/libgstrtpmanager.so"
    "gstreamer-1.0/libgstvideo4linux2.so"
    "gstreamer-1.0/libgstvideoconvertscale.so"
    "gstreamer-1.0/libgstvideorate.so"
    "gstreamer-1.0/libgstvolume.so"
    "gstreamer-1.0/libgstvorbis.so"
    "gstreamer-1.0/libgstvpx.so"
    "gstreamer-1.0/libgstwebrtcdsp.so"
)

set(_qmake "qmake${QT_DEFAULT_MAJOR_VERSION}")
find_program(_qmake_executable NAMES "${_qmake}" REQUIRED)

function(get_qt_path _IN_VARIABLE _OUT_VARIABLE)
    execute_process(
        COMMAND "${_qmake_executable}" -query ${_IN_VARIABLE}
        OUTPUT_VARIABLE OUT_VARIABLE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    if(OUT_VARIABLE)
        message(STATUS "${_OUT_VARIABLE} ${OUT_VARIABLE}")
        set(${_OUT_VARIABLE} "${OUT_VARIABLE}" PARENT_SCOPE)
    endif()
endfunction()

function(install_appimage_runtime_deps)
    set(_output_dir "${CMAKE_CURRENT_BINARY_DIR}/psi")
    set(_app "${_output_dir}/${_CPACK_PACKAGE_NAME}")
    set(_psi_plugin_dir "${_output_dir}/plugins")
    set(_lib_dir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    if(BUILD_PSIMEDIA)
        set(_psimedia_lib "${_psi_plugin_dir}/libmediaplugin.so")
    endif()
    get_qt_path(QT_INSTALL_DATA _qt_install_data)
    get_qt_path(QT_INSTALL_TRANSLATIONS _qt_translations_dir)
    get_qt_path(QT_INSTALL_LIBEXECS _qt_libexec_dir)
    get_qt_path(QT_INSTALL_BINS _qt_bins_dir)
    get_qt_path(QT_INSTALL_ARCHDATA _qt_arch_dir)

    set(_plugin_dir "${_qt_arch_dir}/plugins")

    # Paths may vary across different Qt builds — check several
    # common locations for the WebEngine resources directory.
    set(_qtwebengine_data_candidates
        "${_qt_install_data}/resources"
        "${_qt_install_data}/qtwebengine_resources"
        "${_qt_install_data}/resources/qtwebengine"
    )
    set(_qtwebengine_data_dir "")
    foreach(_candidate IN LISTS _qtwebengine_data_candidates)
        if(EXISTS "${_candidate}/qtwebengine_resources.pak"
           OR EXISTS "${_candidate}/icudtl.dat")
            set(_qtwebengine_data_dir "${_candidate}")
            break()
        endif()
    endforeach()

    # Pass values ​​to install(CODE) so that the code executes during installation.
    set(_deploy_code [=[
if(CMAKE_VERSION GREATER_EQUAL 4.3.0)
    if(POLICY CMP0207)
        cmake_policy(SET CMP0207 NEW)
    endif()
endif()
set(_executables "@_app@")
set(_scan_targets "${_executables}")
set(_plugin_dir "@_plugin_dir@")
set(_qt_plugin_patterns "@DEPLOY_QT_PLUGIN_PATTERNS@")
set(_gst_plugin_patterns "@DEPLOY_GSTREAMER_PLUGIN_PATTERNS@")
set(_webengine_data_dir "@_qtwebengine_data_dir@")
set(_qt_translations_dir "@_qt_translations_dir@")
set(_qt_libexec_dir "@_qt_libexec_dir@")
set(_qt_bins_dir "@_qt_bins_dir@")
set(_lib_dir "@_lib_dir@")
set(_build_psimedia @BUILD_PSIMEDIA@)

set(_plugins_to_scan)

message(STATUS "CPack: get runtime dependencies")

# Add only selected Qt plugins. file(GLOB) expands wildcards,
# for example, platforms/libxcb*.so.
# except qca3 plugins path which set as full pattern
foreach(_pattern IN LISTS _qt_plugin_patterns)
    if(_pattern MATCHES "^/usr/")
        file(GLOB _matches LIST_DIRECTORIES FALSE
        "${_pattern}")
    else()
        file(GLOB _matches LIST_DIRECTORIES FALSE
        "${_plugin_dir}/${_pattern}")
    endif()
    list(APPEND _plugins_to_scan ${_matches})
endforeach()

# Optional media plugin.
if(_build_psimedia)
    if(EXISTS "@_psimedia_lib@")
        list(APPEND _scan_targets @_psimedia_lib@)
    endif()

    # GStreamer plugins are searched for recursively in the application's plugin directory.
    foreach(_pattern IN LISTS _gst_plugin_patterns)
        file(GLOB _matches LIST_DIRECTORIES FALSE
            "${_lib_dir}/${_pattern}")
        list(APPEND _plugins_to_scan ${_matches})
    endforeach()
endif()

list(REMOVE_DUPLICATES _plugins_to_scan)
list(APPEND _scan_targets ${_plugins_to_scan})


# First, scan the application to determine whether WebEngine support is required.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${_executables}"
    RESOLVED_DEPENDENCIES_VAR _app_deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    POST_EXCLUDE_REGEXES
        ".*/ld-linux[^/]*\\.so.*"
        ".*/libc\\.so.*"
        ".*/libm\\.so.*"
        ".*/libpthread\\.so.*"
        ".*/libdl\\.so.*"
        ".*/librt\\.so.*"
)
list(APPEND _all_deps ${_app_deps})

set(_needs_webengine FALSE)
foreach(_dep IN LISTS _app_deps)
    if(_dep MATCHES "Qt[56]WebEngine(Core)?")
        set(_needs_webengine TRUE)
        break()
    endif()
endforeach()

# If there is a dependency on QtWebEngine, look for QtWebEngineProcess near Qt.
# It is usually located in Qt's libexec; also check the Qt bin directory.
if(_needs_webengine)
    find_file(_webengine_process
        NAMES QtWebEngineProcess
        PATHS "${_qt_libexec_dir}" "${_qt_bins_dir}"
        NO_DEFAULT_PATH
        REQUIRED
    )
    list(APPEND _executables "${_webengine_process}")
    list(APPEND _scan_targets "${_webengine_process}")
endif()

# Supplement the set with executable and library targets.
# Scan each target individually: LIBRARIES is used for libraries,
# and EXECUTABLES for executables.
foreach(_target IN LISTS _scan_targets)
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR "Runtime dependency scan target does not exist: ${_target}")
    endif()

    if(_target MATCHES "\\.(so(\\.[0-9.]+)?)$")
        file(GET_RUNTIME_DEPENDENCIES
            LIBRARIES "${_target}"
            RESOLVED_DEPENDENCIES_VAR _deps
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            POST_EXCLUDE_REGEXES
                ".*/ld-linux[^/]*\\.so.*"
                ".*/libc\\.so.*"
                ".*/libm\\.so.*"
                ".*/libpthread\\.so.*"
                ".*/libdl\\.so.*"
                ".*/librt\\.so.*"
        )
    else()
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${_target}"
            RESOLVED_DEPENDENCIES_VAR _deps
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            POST_EXCLUDE_REGEXES
                ".*/ld-linux[^/]*\\.so.*"
                ".*/libc\\.so.*"
                ".*/libm\\.so.*"
                ".*/libpthread\\.so.*"
                ".*/libdl\\.so.*"
                ".*/librt\\.so.*"
        )
    endif()

    list(APPEND _all_deps ${_deps})
endforeach()

list(REMOVE_DUPLICATES _all_deps)

# It is best not to ignore unresolved dependencies: otherwise, the AppImage might
# build successfully but fail to launch on the target system.
if(_unresolved)
    message(WARNING "Unresolved runtime dependencies: ${_unresolved}")
endif()

set(_lib_dest "${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_LIBDIR@")
file(MAKE_DIRECTORY "${_lib_dest}")
set(_bin_dest "${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_BINDIR@")
file(MAKE_DIRECTORY "${_bin_dest}")

foreach(_dep IN LISTS _all_deps)
    if(NOT EXISTS "${_dep}")
        continue()
    endif()

    # Preserve the original name/link, then copy the actual file.
    file(INSTALL DESTINATION "${_lib_dest}" TYPE FILE FILES "${_dep}")
    file(REAL_PATH "${_dep}" _real_dep)
    if(NOT _real_dep STREQUAL _dep)
        file(INSTALL DESTINATION "${_lib_dest}" TYPE FILE FILES "${_real_dep}")
    endif()
endforeach()

# Install qt plugins and gstreamer plugins if BUILD_PSIMEDIA enabled
foreach(_dep_plugin IN LISTS _plugins_to_scan)
    string(FIND "${_dep_plugin}" "gstreamer" gstpos)
    string(FIND "${_dep_plugin}" "/usr/lib/qca3-qt@QT_DEFAULT_MAJOR_VERSION@" qcapos)
    set(_destination_prefix "${_lib_dest}/qt@QT_DEFAULT_MAJOR_VERSION@/plugins")
    if(NOT gstpos EQUAL -1)
        file(RELATIVE_PATH _plugin_relative
            "${_lib_dir}"
            "${_dep_plugin}"
        )
        set(_destination_prefix "${_lib_dest}")
    elseif(NOT qcapos EQUAL -1)
        file(RELATIVE_PATH _plugin_relative
            "/usr/lib/qca3-qt@QT_DEFAULT_MAJOR_VERSION@"
            "${_dep_plugin}"
        )
    else()
        file(RELATIVE_PATH _plugin_relative
            "${_plugin_dir}"
            "${_dep_plugin}"
        )
    endif()
    get_filename_component(_plugin_subdir "${_plugin_relative}" DIRECTORY)
    file(INSTALL DESTINATION "${_destination_prefix}/${_plugin_subdir}" TYPE FILE FILES "${_dep_plugin}")
endforeach()

# QtWebEngineProcess is installed alongside the main binary.
if(_needs_webengine)
    file(INSTALL DESTINATION "${_bin_dest}" TYPE FILE FILES "${_webengine_process}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

# WebEngine resources and translations are kept separate, under share/${PROJECT_NAME}.
set(_data_dest
    "${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_DATAROOTDIR@/@PROJECT_NAME@")

if(_needs_webengine)
    if(_webengine_data_dir AND EXISTS "${_webengine_data_dir}")
        file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_DATAROOTDIR@/resources"
            TYPE DIRECTORY FILES "${_webengine_data_dir}/")
    else()
        message(WARNING
            "QtWebEngine is used, but its resources directory was not found")
    endif()

    if(_qt_translations_dir AND EXISTS "${_qt_translations_dir}")
        file(GLOB _webengine_translations
            "${_qt_translations_dir}/qtwebengine_locales"
            "${_qt_translations_dir}/qtwebengine_*.qm")
        if(_webengine_translations)
            file(INSTALL DESTINATION "${_data_dest}/translations"
                TYPE FILE FILES ${_webengine_translations})
        endif()
    endif()
endif()

file(GLOB _qt_translations "${_qt_translations_dir}/qt*.qm")
if(_qt_translations)
    file(INSTALL DESTINATION "${_data_dest}/translations"
        TYPE FILE FILES ${_qt_translations})
endif()

]=])

    # Substitution of paths calculated during configuration.
    string(CONFIGURE "${_deploy_code}" _deploy_code @ONLY)
    install(CODE "${_deploy_code}")
endfunction()
