unset(MOC_SOURCES)
unset(HEADERS)
unset(PLAIN_HEADERS)
unset(FORMS)
unset(SOURCES)
unset(PLAIN_SOURCES)
unset(UI_FORMS)

include_directories(
	${PROJECT_SOURCE_DIR}/iris/include/iris
	${PROJECT_SOURCE_DIR}/iris/src
)

list(APPEND FORMS
	CertificateDisplay.ui
)

list(APPEND HEADERS
	CertificateDisplayDialog.h
	CertificateErrorDialog.h
)

list(APPEND PLAIN_HEADERS
	CertificateHelpers.h
)

list(APPEND PLAIN_SOURCES
	CertificateDisplayDialog.cpp
	CertificateErrorDialog.cpp
	CertificateHelpers.cpp
)

qt4_wrap_ui(UI_FORMS ${FORMS})
qt_wrap_cpp(MOC_SOURCES ${HEADERS})
add_library(Certificates STATIC ${HEADERS} ${MOC_SOURCES} ${UI_FORMS} ${PLAIN_SOURCES})
target_link_libraries(Certificates ${QT_LIBRARIES} ${iris_LIB})
target_include_directories(Certificates PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
