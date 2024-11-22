
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
add_compile_options("-Wextra")
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

# aux
set(FN_LIST secure_getenv localtime_r uselocale)
foreach(fn IN LISTS FN_LIST)
  string(TOUPPER "HAVE_${fn}" FN_DEF)
  check_library_exists(c "${fn}" "${CMAKE_LIBRARY_PATH}" "${FN_DEF}")
  if("${${FN_DEF}}")
    set("${FN_DEF}" ON)
  endif()
endforeach()

# optional: DND (default ON)
option(DND "DND support" ON)
if(DND)
  set("WITH_DND" ON)
endif()

# optional: JSON (default ON)
option(JSON "JSON support in recap mode (summary at exit)" ON)
if(JSON)
  set("WITH_JSON" ON)
  pkg_check_modules(JSON REQUIRED IMPORTED_TARGET json-glib-1.0)
  link_libraries(PkgConfig::JSON)
endif()

# optional: PLOT (default ON)
option(PLOT "PLOT With 3D plots" ON)
if(PLOT)
  set("WITH_PLOT" ON)
  list(APPEND SRCS tabs/plot.c tabs/plot_aux.c tabs/plot_pango.c)
  pkg_check_modules(GL REQUIRED IMPORTED_TARGET gl)
  link_libraries(PkgConfig::GL)
  pkg_check_modules(CGLM REQUIRED IMPORTED_TARGET cglm)
  link_libraries(PkgConfig::CGLM)
  pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)
  link_libraries(PkgConfig::EPOXY)
endif()

option(pingdir "Path to ping" OFF)
if(pingdir)
  set("PINGDIR" "${pingdir}")
endif()

set(CONFIG "config.h")
configure_file("${CONFIG}.cmake" "${CONFIG}" @ONLY)

add_executable("${PROJECT_NAME}" ${SRCS})
install(TARGETS "${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "assets/${PROJECT_NAME}.desktop" RENAME "net.tools.${PROJECT_NAME}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

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

