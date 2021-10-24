cmake_minimum_required(VERSION 3.9)

project("gif" VERSION 0.1.0)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        gif_lib.c
)

set(public_headers
    gif_lib.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES 
    PUBLIC_HEADER "${public_headers}"
    VERSION ${PROJECT_VERSION}
    SOVERSION 0)

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
