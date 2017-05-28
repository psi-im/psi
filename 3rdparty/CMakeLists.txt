cmake_minimum_required(VERSION 3.1.0)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

project(qhttp)

add_definitions(
	-DQHTTP_MEMORY_LOG=0
)
if(WIN32)
	add_definitions(
		-D_WINDOWS
		-DWIN32_LEAN_AND_MEAN
		-DNOMINMAX
		-DQHTTP_EXPORT
	)
endif()

include_directories(
	.
	./qhttp/src
	./qhttp/src/private
	./http-parser
)

find_package(Qt5 REQUIRED Core Network)

set(http_parser_srcs
	http-parser/http_parser.c
)

set(http_parser_hdrs
	http-parser/http_parser.h
)

set(qhttp_srcs
	qhttp/src/qhttpabstracts.cpp
	qhttp/src/qhttpserverconnection.cpp
	qhttp/src/qhttpserver.cpp
	qhttp/src/qhttpserverrequest.cpp
	qhttp/src/qhttpserverresponse.cpp

)

set(qhttp_hdrs
	qhttp/src/qhttpfwd.hpp
	qhttp/src/qhttpabstracts.hpp
	qhttp/src/qhttpserverconnection.hpp
	qhttp/src/qhttpserver.hpp
	qhttp/src/qhttpserverrequest.hpp
	qhttp/src/qhttpserverresponse.hpp

)

set(qhttp_priv_hdrs
	qhttp/src/private/httpparser.hxx
	qhttp/src/private/qhttpclient_private.hpp
	qhttp/src/private/qhttpserver_private.hpp
	qhttp/src/private/httpreader.hxx
	qhttp/src/private/qhttpclientrequest_private.hpp
	qhttp/src/private/qhttpserverrequest_private.hpp
	qhttp/src/private/httpwriter.hxx
	qhttp/src/private/qhttpclientresponse_private.hpp
	qhttp/src/private/qhttpserverresponse_private.hpp
	qhttp/src/private/qhttpbase.hpp
	qhttp/src/private/qhttpserverconnection_private.hpp
	qhttp/src/private/qsocket.hpp
)

qt5_wrap_cpp(MOCS
	qhttp/src/qhttpabstracts.hpp
	qhttp/src/qhttpserverconnection.hpp
	qhttp/src/qhttpserver.hpp
	qhttp/src/qhttpserverrequest.hpp
	qhttp/src/qhttpserverresponse.hpp
)

add_library(qhttp
	STATIC
	${http_parser_srcs}
	${http_parser_hdrs}
	${qhttp_srcs}
	${MOCS}
)

set_property(TARGET qhttp PROPERTY CXX_STANDARD 14)
set_property(TARGET qhttp PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(qhttp Qt5::Core Qt5::Network)
target_include_directories(qhttp
	PUBLIC
	./qhttp/src
	./qhttp/src/private
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)
