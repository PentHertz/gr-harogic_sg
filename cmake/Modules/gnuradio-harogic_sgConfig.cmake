find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_HAROGIC_SG gnuradio-harogic_sg)

FIND_PATH(
    GR_HAROGIC_SG_INCLUDE_DIRS
    NAMES gnuradio/harogic_sg/api.h
    HINTS $ENV{HAROGIC_SG_DIR}/include
        ${PC_HAROGIC_SG_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_HAROGIC_SG_LIBRARIES
    NAMES gnuradio-harogic_sg
    HINTS $ENV{HAROGIC_SG_DIR}/lib
        ${PC_HAROGIC_SG_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-harogic_sgTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_HAROGIC_SG DEFAULT_MSG GR_HAROGIC_SG_LIBRARIES GR_HAROGIC_SG_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_HAROGIC_SG_LIBRARIES GR_HAROGIC_SG_INCLUDE_DIRS)
