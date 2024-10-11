
cmake_minimum_required(VERSION 3.12)
project(pingpath C)

find_package(PkgConfig REQUIRED)
include(CheckLibraryExists)
include(GNUInstallDirs)

#set(CMAKE_C_STANDARD 23) # for compat tests
#set(CMAKE_EXPORT_COMPILE_COMMANS ON) # compile_commands.json
include_directories(${CMAKE_SOURCE_DIR})

set(MAN_PAGE "${PROJECT_NAME}.1")
set(MAN_PATH "${CMAKE_BINARY_DIR}/man1")
set(MANUAL "${MAN_PATH}/${MAN_PAGE}")

set(SRCS "${PROJECT_NAME}.c" common.c
  pinger.c parser.c stat.c series.c dns.c whois.c cli.c
  ui/style.c ui/appbar.c ui/action.c ui/option.c ui/clipboard.c ui/notifier.c
  tabs/aux.c tabs/ping.c tabs/graph.c tabs/log.c)

# mandatory
add_compile_options("-Wall")
check_library_exists(c sqrt "${CMAKE_LIBRARY_PATH}" HAVE_SQRT_IN_C)
if(NOT HAVE_SQRT_IN_C)
  check_library_exists(m sqrt "${CMAKE_LIBRARY_PATH}" HAVE_SQRT_IN_M)
  if(NOT HAVE_SQRT_IN_M)
    message(FATAL_ERROR "No suitable math found")
  endif()
  link_libraries("-lm")
endif()
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
link_libraries(PkgConfig::GTK4)

# optional: DND (default ON)
option(DND "DND support" ON)
if(DND)
  add_compile_options("-DWITH_DND")
endif()

# optional: JSON (default ON)
option(JSON "JSON support in recap mode (summary at exit)" ON)
if(JSON)
  add_compile_options("-DWITH_JSON")
  pkg_check_modules(JSON REQUIRED IMPORTED_TARGET json-glib-1.0)
  link_libraries(PkgConfig::JSON)
endif()

set(NAME2 "${PROJECT_NAME}2")
add_executable("${NAME2}" ${SRCS})
install(TARGETS "${NAME2}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "assets/${NAME2}.desktop" RENAME "net.tools.${PROJECT_NAME}2.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

# optional: PLOT (default ON)
option(PLOT "PLOT With 3D plots" ON)
if(PLOT)
  set(PP_SRC ${SRCS})
  set(SRC3D tabs/plot.c tabs/plot_aux.c tabs/plot_pango.c)
  list(APPEND PP_SRC ${SRC3D})
  add_executable("${PROJECT_NAME}" ${PP_SRC})
  pkg_check_modules(GL REQUIRED IMPORTED_TARGET gl)
  target_link_libraries("${PROJECT_NAME}" PRIVATE PkgConfig::GL)
  pkg_check_modules(CGLM REQUIRED IMPORTED_TARGET cglm)
  target_link_libraries("${PROJECT_NAME}" PRIVATE PkgConfig::CGLM)
  pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)
  target_link_libraries("${PROJECT_NAME}" PRIVATE PkgConfig::EPOXY)
  target_compile_options("${PROJECT_NAME}" PRIVATE -DWITH_PLOT)
  install(TARGETS "${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(FILES "assets/${PROJECT_NAME}.desktop" RENAME "net.tools.${PROJECT_NAME}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/${PROJECT_NAME}.svg" DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
install(FILES "${PROJECT_NAME}.conf.sample" RENAME "${PROJECT_NAME}.conf" DESTINATION "${CMAKE_INSTALL_DOCDIR}/examples")

file(MAKE_DIRECTORY "${MAN_PATH}")
configure_file("${MAN_PAGE}" "${MANUAL}" COPYONLY)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.14.0")
  install(DIRECTORY "${MAN_PATH}" TYPE MAN)
else()
  install(DIRECTORY "${MAN_PATH}" DESTINATION ${CMAKE_INSTALL_MANDIR})
endif()

message("")
if(CMAKE_BUILD_TYPE)
  message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "Options:")
message(STATUS "  DND  ${DND}\t: DND support")
message(STATUS "  JSON ${JSON}\t: JSON support")
message(STATUS "  PLOT ${PLOT}\t: 3D plots")
message("")

