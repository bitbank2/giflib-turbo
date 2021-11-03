cmake_minimum_required(VERSION 3.11)

project("gif" VERSION 0.1.0)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        gif_lib.c
)

set(PUBLIC_HEADERS
    gif_lib.h
)

set(LIB_VERSION_MAJOR 7)
set(LIB_VERSION_MINOR 2)
set(LIB_VERSION_PATCH 0)
set(LIB_VERSION "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}")

set_target_properties(${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    VERSION ${LIB_VERSION}
    SOVERSION ${LIB_VERSION_MAJOR})

IF(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_NAME "${PROJECT_NAME}-${LIB_VERSION_MAJOR}")
ENDIF(WIN32)

configure_file(giflib.pc.in giflib.pc @ONLY)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME} EXPORT "${PROJECT_NAME}Config"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${CMAKE_BINARY_DIR}/giflib.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

include(CMakePackageConfigHelpers)

install(EXPORT "${PROJECT_NAME}Config"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/giflib
)

add_executable(test)
target_sources(test PRIVATE test.c)
target_link_libraries(test PRIVATE ${PROJECT_NAME})
