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
        "sweep (2), level sweep (3) or built-in waveforms: AM (4), FM (5), PM (6), "
        "pulse (7), multitone (8), ramp sweep (9), AWGN (10), digital modulation (11), "
        "DSSS (12), OFDM (13)")

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
             py::arg("max_latency_ms") = 50.0,
             py::arg("mod_rate") = 1e3,
             py::arg("mod_shape") = 1,
             py::arg("am_depth") = 50.0,
             py::arg("fm_deviation") = 10e3,
             py::arg("pm_deviation") = 1.0,
             py::arg("pulse_width") = 10e-6,
             py::arg("pulse_period") = 100e-6,
             py::arg("tone_count") = 8,
             py::arg("tone_spacing") = 100e3,
             py::arg("tone_phase") = 2,
             py::arg("ramp_span") = 1e6,
             py::arg("ramp_time") = 1e-3,
             py::arg("ramp_period") = 1e-3,
             py::arg("noise_bw") = 1e6,
             py::arg("noise_length") = 10e-3,
             py::arg("dig_type") = 3,
             py::arg("symbol_rate") = 100e3,
             py::arg("fsk_deviation") = 25e3,
             py::arg("filter_type") = 3,
             py::arg("filter_alpha") = 0.35,
             py::arg("sps") = 4,
             py::arg("filter_span") = 8,
             py::arg("pn_order") = 9,
             py::arg("pn_seed") = 1,
             py::arg("symbols") = 0,
             py::arg("dsss_code") = 5,
             py::arg("ofdm_fft") = 64,
             py::arg("ofdm_symbols") = 100,
             py::arg("ofdm_guard_left") = 6,
             py::arg("ofdm_guard_right") = 5,
             py::arg("ofdm_cp") = 25,
             py::arg("ofdm_window") = 0,
             py::arg("ofdm_null_dc") = true,
             py::arg("ofdm_mod") = 3,
             py::arg("ofdm_idle") = 0.0,
             py::arg("ofdm_rate") = 20e6)

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
        .def("set_analog_mod",
             &sink::set_analog_mod,
             py::arg("rate"),
             py::arg("shape"),
             py::arg("am_depth"),
             py::arg("fm_deviation"),
             py::arg("pm_deviation"))
        .def("set_pulse", &sink::set_pulse, py::arg("width"), py::arg("period"))
        .def("set_multitone",
             &sink::set_multitone,
             py::arg("count"),
             py::arg("spacing"),
             py::arg("phase_mode"))
        .def("set_ramp_sweep",
             &sink::set_ramp_sweep,
             py::arg("span"),
             py::arg("sweep_time"),
             py::arg("period"))
        .def("set_noise", &sink::set_noise, py::arg("bandwidth"), py::arg("length"))
        .def("set_digital_mod",
             &sink::set_digital_mod,
             py::arg("type"),
             py::arg("symbol_rate"),
             py::arg("fsk_deviation"),
             py::arg("filter_type"),
             py::arg("alpha"),
             py::arg("sps"),
             py::arg("filter_span"),
             py::arg("pn_order"),
             py::arg("pn_seed"),
             py::arg("symbols"))
        .def("set_dsss_code", &sink::set_dsss_code, py::arg("code"))
        .def("set_ofdm",
             &sink::set_ofdm,
             py::arg("fft_size"),
             py::arg("symbols"),
             py::arg("guard_left"),
             py::arg("guard_right"),
             py::arg("cp_percent"),
             py::arg("window_percent"),
             py::arg("null_dc"),
             py::arg("subcarrier_mod"),
             py::arg("idle_time"),
             py::arg("samp_rate"))
        .def("playback_rate", &sink::playback_rate)
        .def("playback_points", &sink::playback_points)
        .def("mode", &sink::mode)
        .def("frequency", &sink::frequency)
        .def("level", &sink::level)
        .def("samp_rate", &sink::samp_rate)
        .def("rf_on", &sink::rf_on)
        .def("underruns", &sink::underruns);
}
