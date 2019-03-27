unset(HEADERS)
unset(FORMS)
unset(SOURCES)
unset(UI_FORMS)

list(APPEND HEADERS
    privacydlg.h
    privacymanager.h
    privacyruledlg.h
    psiprivacymanager.h

    privacylist.h
    privacylistblockedmodel.h
    privacylistitem.h
    privacylistmodel.h
)

list(APPEND SOURCES
    psiprivacymanager.cpp

    privacydlg.cpp
    privacylist.cpp
    privacylistblockedmodel.cpp
    privacylistitem.cpp
    privacylistmodel.cpp
    privacyruledlg.cpp
)

list(APPEND FORMS
    privacy.ui
    privacyrule.ui
)

qt5_wrap_ui(UI_FORMS ${FORMS})
add_library(privacy STATIC ${SOURCES} ${HEADERS} ${UI_FORMS})
target_link_libraries(privacy ${QT_LIBRARIES} ${iris_LIB} libpsi_tools widgets tabs)
target_include_directories(privacy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
