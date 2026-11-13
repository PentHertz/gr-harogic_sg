#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The presence of this file turns this directory into a Python package

'''
GNU Radio blocks for Harogic H2 signal generators:
  harogic_sg.sink - GNU Radio IQ (real time), CW, frequency sweep or level sweep
'''
import os

# import pybind11 generated symbols into the harogic_sg namespace
try:
    # this might fail if the module is python-only
    from .harogic_sg_python import *
except ModuleNotFoundError:
    pass

# import any pure python here
#
