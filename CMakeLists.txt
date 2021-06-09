cmake_minimum_required(VERSION 3.16.0)

project(hb-server VERSION 2.0.0 LANGUAGES C)

# Options
option(DEBUG "Build with debugging messages and tools" ON)
if (DEBUG)
	add_definitions(-DDEBUG)
endif()
option(SECURE_SOCKETS "Build with SSL sockets instead of plain sockets" ON)
if (SECURE_SOCKETS)
	add_definitions(-DSSLSOCK)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

if (UNIX AND NOT APPLE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -pthread")
endif()

# Curl
find_package(CURL REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)

# pkg-config packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARGON2 REQUIRED libargon2)
pkg_check_modules(MARIADB REQUIRED libmariadb)
pkg_check_modules(MSGPACK REQUIRED msgpack)

set(HEADERS
	src/hbp.h
	src/herbank.h
)

set(SOURCES
	#src/iban.c
	src/noob.c
	src/transfer.c
	src/balance.c
	src/info.c
	src/login.c
	src/session.c
	src/main.c
)

add_executable(${PROJECT_NAME}
	${HEADERS}
	${SOURCES}
)
target_link_libraries(${PROJECT_NAME}
	${ARGON2_LINK_LIBRARIES}
	${CURL_LIBRARIES}
	${MARIADB_LINK_LIBRARIES}
	${MSGPACK_LINK_LIBRARIES}
	${OPENSSL_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PUBLIC
	${ARGON2_INCLUDE_DIRS}
	${CURL_INCLUDE_DIR}
	${MARIADB_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIR}
)
target_compile_options(${PROJECT_NAME} PUBLIC
	${ARGON2_CFLAGS_OTHER}
	${MARIADB_CFLAGS_OTHER}
	${MSGPACK_CFLAGS_OTHER}
)
