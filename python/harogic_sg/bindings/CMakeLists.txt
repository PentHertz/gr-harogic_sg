# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT harogic_sg_sources)
    message(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT harogic_sg_sources)

########################################################################
# Check for pygccxml
########################################################################
gr_python_check_module_raw("pygccxml" "import pygccxml" PYGCCXML_FOUND)

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND harogic_sg_python_files
    sink_python.cc
    python_bindings.cc)

gr_pybind_make_oot(harogic_sg ../../.. gr::harogic_sg "${harogic_sg_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(
    TARGET harogic_sg_python
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:harogic_sg_python>
            ${PROJECT_BINARY_DIR}/test_modules/gnuradio/harogic_sg/)

install(
    TARGETS harogic_sg_python
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/harogic_sg
    COMPONENT pythonapi)
