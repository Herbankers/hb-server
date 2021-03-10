cmake_minimum_required(VERSION 3.16.0)

project(kech-server VERSION 2.0.0 LANGUAGES C)

# Options
option(DEBUG "Build with debugging messages and tools" ON)
if (DEBUG)
	add_definitions(-DDEBUG)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

set(CMAKE_VERBOSE_MAKEFILE TRUE)

# OpenSSL
find_package(OpenSSL REQUIRED)

# MariaDB
find_package(PkgConfig)
pkg_check_modules(MARIADB REQUIRED mariadb)

set(HEADERS
	src/kbp.h
	src/kech.h
)

set(SOURCES
	src/accounts.c
	src/iban.c
	src/login.c
	src/main.c
	src/pin_update.c
	src/session.c
	src/transactions.c
	src/transfer.c
)

add_executable(${PROJECT_NAME}
	${HEADERS}
	${SOURCES}
)

link_directories(${MARIADB_LIBRARY_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${MARIADB_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${MARIADB_LIBRARIES})
#target_include_directories(${PROJECT_NAME} PRIVATE include)
