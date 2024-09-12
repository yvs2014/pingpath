
cmake_minimum_required(VERSION 3.5)
project(pingpath C)

find_package(PkgConfig REQUIRED)
include(GNUInstallDirs)

#set(CMAKE_C_STANDARD 23) # for compat tests
include_directories(${CMAKE_SOURCE_DIR})

SET(EXECS ${PROJECT_NAME}2 ${PROJECT_NAME})
SET(SRC_C ${PROJECT_NAME}.c common.c
  pinger.c parser.c stat.c series.c dns.c whois.c cli.c
  ui/style.c ui/appbar.c ui/action.c ui/option.c
  ui/clipboard.c ui/notifier.c
  tabs/aux.c tabs/ping.c tabs/graph.c tabs/log.c)
SET(SRC3D_C tabs/plot.c tabs/plot_aux.c tabs/plot_pango.c)

# mandatory: GTK4
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

# optional: DND (default ON)
option(DND "DND support" ON)

# optional: JSON (default ON)
option(JSON "JSON support in recap mode (summary at exit)" ON)
if(JSON)
  pkg_check_modules(JSON REQUIRED IMPORTED_TARGET json-glib-1.0)
endif()

# build binaries
foreach(PP_NAME ${EXECS})
  set(PP_SRC ${SRC_C})
  if(PP_NAME STREQUAL "${PROJECT_NAME}")
    list(APPEND PP_SRC ${SRC3D_C})
  endif()
  add_executable(${PP_NAME} ${PP_SRC})

  target_compile_options(${PP_NAME} PRIVATE -Wall)
  if(PP_NAME STREQUAL "${PROJECT_NAME}")
    pkg_check_modules(GL REQUIRED IMPORTED_TARGET gl)
    pkg_check_modules(CGLM REQUIRED IMPORTED_TARGET cglm)
    pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)
    target_link_libraries(${PP_NAME} PRIVATE PkgConfig::GL)
    target_link_libraries(${PP_NAME} PRIVATE PkgConfig::CGLM)
    target_link_libraries(${PP_NAME} PRIVATE PkgConfig::EPOXY)
    target_compile_options(${PP_NAME} PRIVATE -DWITH_PLOT)
  endif()
  target_link_libraries(${PP_NAME} PRIVATE -lm)
  target_link_libraries(${PP_NAME} PRIVATE PkgConfig::GTK4)
  if(JSON)
    target_compile_options(${PP_NAME} PRIVATE -DWITH_JSON)
    target_link_libraries(${PP_NAME} PRIVATE PkgConfig::JSON)
  endif()
  if(DND)
    target_compile_options(${PP_NAME} PRIVATE -DWITH_DND)
  endif()
  install(TARGETS ${PP_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES assets/${PP_NAME}.desktop RENAME net.tools.${PP_NAME}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endforeach(PP_NAME ${EXECS})

file(ARCHIVE_CREATE OUTPUT ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.1.gz PATHS ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.1 FORMAT raw COMPRESSION GZip)
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.1.gz DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES ${PROJECT_NAME}.conf.sample RENAME ${PROJECT_NAME}.conf DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/${PROJECT_NAME}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

