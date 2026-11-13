/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/***********************************************************************************/
/* This file was hand-written; keep bindtool from regenerating it.                 */
/* BINDTOOL_GEN_AUTOMATIC(0)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(sink.h)                                                    */
/* BINDTOOL_HEADER_FILE_HASH(0)                                                    */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/harogic_sg/sink.h>

void bind_sink(py::module& m)
{
    using sink = gr::harogic_sg::sink;

    py::class_<sink, gr::sync_block, gr::block, gr::basic_block, std::shared_ptr<sink>>(
        m,
        "sink",
        "Harogic H2 signal generator sink: GNU Radio IQ (mode 0), CW (1), frequency "
        "sweep (2) or level sweep (3)")

        .def(py::init(&sink::make),
             py::arg("device_index") = 0,
             py::arg("mode") = 0,
             py::arg("frequency") = 868.3e6,
             py::arg("level") = -30.0,
             py::arg("samp_rate") = 1e6,
             py::arg("rf_on") = true,
             py::arg("scale") = 1.0,
             py::arg("sweep_start") = 868.0e6,
             py::arg("sweep_stop") = 868.6e6,
             py::arg("sweep_step") = 100e3,
             py::arg("level_start") = -40.0,
             py::arg("level_stop") = -10.0,
             py::arg("level_step") = 1.0,
             py::arg("dwell") = 0.1,
             py::arg("freq_offset") = 0.0,
             py::arg("phase_offset") = 0.0,
             py::arg("lo_mode") = 0,
             py::arg("rf_port") = 0,
             py::arg("ext_ref") = false,
             py::arg("ref_freq") = 10e6,
             py::arg("ref_out") = false,
             py::arg("max_latency_ms") = 50.0)

        .def("set_mode", &sink::set_mode, py::arg("mode"))
        .def("set_frequency", &sink::set_frequency, py::arg("frequency"))
        .def("set_level", &sink::set_level, py::arg("level"))
        .def("set_samp_rate", &sink::set_samp_rate, py::arg("samp_rate"))
        .def("set_rf_on", &sink::set_rf_on, py::arg("rf_on"))
        .def("set_scale", &sink::set_scale, py::arg("scale"))
        .def("set_sweep", &sink::set_sweep, py::arg("start"), py::arg("stop"), py::arg("step"))
        .def("set_level_sweep", &sink::set_level_sweep, py::arg("start"), py::arg("stop"), py::arg("step"))
        .def("set_dwell", &sink::set_dwell, py::arg("dwell"))
        .def("set_freq_offset", &sink::set_freq_offset, py::arg("freq_offset"))
        .def("set_phase_offset", &sink::set_phase_offset, py::arg("phase_offset"))
        .def("set_lo_mode", &sink::set_lo_mode, py::arg("lo_mode"))
        .def("set_max_latency_ms", &sink::set_max_latency_ms, py::arg("max_latency_ms"))
        .def("mode", &sink::mode)
        .def("frequency", &sink::frequency)
        .def("level", &sink::level)
        .def("samp_rate", &sink::samp_rate)
        .def("rf_on", &sink::rf_on)
        .def("underruns", &sink::underruns);
}
