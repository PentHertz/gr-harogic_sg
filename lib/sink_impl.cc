/* -*- c++ -*- */
/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sink_impl.h"
#include <gnuradio/io_signature.h>
#include <volk/volk.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace gr {
namespace harogic_sg {

static constexpr double MIN_RATE = 195312.5;
static constexpr double MAX_RATE = 62.5e6;

sink::sptr sink::make(int device_index,
                      int mode,
                      double frequency,
                      double level,
                      double samp_rate,
                      bool rf_on,
                      double scale,
                      double sweep_start,
                      double sweep_stop,
                      double sweep_step,
                      double level_start,
                      double level_stop,
                      double level_step,
                      double dwell,
                      double freq_offset,
                      double phase_offset,
                      int lo_mode,
                      int rf_port,
                      bool ext_ref,
                      double ref_freq,
                      bool ref_out,
                      double max_latency_ms,
                      double mod_rate,
                      int mod_shape,
                      double am_depth,
                      double fm_deviation,
                      double pm_deviation,
                      double pulse_width,
                      double pulse_period,
                      int tone_count,
                      double tone_spacing,
                      int tone_phase,
                      double ramp_span,
                      double ramp_time,
                      double ramp_period,
                      double noise_bw,
                      double noise_length,
                      int dig_type,
                      double symbol_rate,
                      double fsk_deviation,
                      int filter_type,
                      double filter_alpha,
                      int sps,
                      int filter_span,
                      int pn_order,
                      int pn_seed,
                      int symbols,
                      int dsss_code,
                      int ofdm_fft,
                      int ofdm_symbols,
                      int ofdm_guard_left,
                      int ofdm_guard_right,
                      int ofdm_cp,
                      int ofdm_window,
                      bool ofdm_null_dc,
                      int ofdm_mod,
                      double ofdm_idle,
                      double ofdm_rate)
{
    return gnuradio::make_block_sptr<sink_impl>(device_index,
                                                mode,
                                                frequency,
                                                level,
                                                samp_rate,
                                                rf_on,
                                                scale,
                                                sweep_start,
                                                sweep_stop,
                                                sweep_step,
                                                level_start,
                                                level_stop,
                                                level_step,
                                                dwell,
                                                freq_offset,
                                                phase_offset,
                                                lo_mode,
                                                rf_port,
                                                ext_ref,
                                                ref_freq,
                                                ref_out,
                                                max_latency_ms,
                                                mod_rate,
                                                mod_shape,
                                                am_depth,
                                                fm_deviation,
                                                pm_deviation,
                                                pulse_width,
                                                pulse_period,
                                                tone_count,
                                                tone_spacing,
                                                tone_phase,
                                                ramp_span,
                                                ramp_time,
                                                ramp_period,
                                                noise_bw,
                                                noise_length,
                                                dig_type,
                                                symbol_rate,
                                                fsk_deviation,
                                                filter_type,
                                                filter_alpha,
                                                sps,
                                                filter_span,
                                                pn_order,
                                                pn_seed,
                                                symbols,
                                                dsss_code,
                                                ofdm_fft,
                                                ofdm_symbols,
                                                ofdm_guard_left,
                                                ofdm_guard_right,
                                                ofdm_cp,
                                                ofdm_window,
                                                ofdm_null_dc,
                                                ofdm_mod,
                                                ofdm_idle,
                                                ofdm_rate);
}

static void check_rate(double r)
{
    if (r < MIN_RATE - 1 || r > MAX_RATE + 1)
        throw std::invalid_argument(
            "harogic_sg.sink: samp_rate must be between 195312.5 and 62.5e6 S/s");
}

static constexpr int FIRST_BUILTIN_MODE = 4;

void sink_impl::set_kind(int mode)
{
    switch (mode) {
    case 0:
        d_plan.kind = tx_plan::REALTIME;
        return;
    case 1:
        d_plan.kind = tx_plan::CW;
        return;
    case 2:
        d_plan.kind = tx_plan::FREQ_SWEEP;
        return;
    case 3:
        d_plan.kind = tx_plan::LEVEL_SWEEP;
        return;
    default:
        if (mode >= FIRST_BUILTIN_MODE &&
            mode < FIRST_BUILTIN_MODE + mod_settings::NUM_WAVES) {
            d_plan.kind = tx_plan::PLAYBACK;
            d_plan.mod.wave = (mod_settings::wave_t)(mode - FIRST_BUILTIN_MODE);
            return;
        }
        throw std::invalid_argument(
            "harogic_sg.sink: mode must be 0 (GNU Radio IQ), 1 (CW), 2 (frequency "
            "sweep), 3 (level sweep), 4 AM, 5 FM, 6 PM, 7 pulse, 8 multitone, 9 ramp "
            "sweep, 10 AWGN, 11 digital modulation, 12 DSSS or 13 OFDM");
    }
}

int sink_impl::mode() const
{
    switch (d_plan.kind) {
    case tx_plan::REALTIME:
        return 0;
    case tx_plan::CW:
        return 1;
    case tx_plan::FREQ_SWEEP:
        return 2;
    case tx_plan::LEVEL_SWEEP:
        return 3;
    default:
        return FIRST_BUILTIN_MODE + (int)d_plan.mod.wave;
    }
}

double sink_impl::playback_rate() const
{
    std::lock_guard<std::mutex> lock(d_mutex);
    return d_dev ? d_dev->playback_rate() : 0.0;
}

uint64_t sink_impl::playback_points() const
{
    std::lock_guard<std::mutex> lock(d_mutex);
    return d_dev ? d_dev->playback_points() : 0;
}

sink_impl::sink_impl(int device_index,
                     int mode,
                     double frequency,
                     double level,
                     double samp_rate,
                     bool rf_on,
                     double scale,
                     double sweep_start,
                     double sweep_stop,
                     double sweep_step,
                     double level_start,
                     double level_stop,
                     double level_step,
                     double dwell,
                     double freq_offset,
                     double phase_offset,
                     int lo_mode,
                     int rf_port,
                     bool ext_ref,
                     double ref_freq,
                     bool ref_out,
                     double max_latency_ms,
                     double mod_rate,
                     int mod_shape,
                     double am_depth,
                     double fm_deviation,
                     double pm_deviation,
                     double pulse_width,
                     double pulse_period,
                     int tone_count,
                     double tone_spacing,
                     int tone_phase,
                     double ramp_span,
                     double ramp_time,
                     double ramp_period,
                     double noise_bw,
                     double noise_length,
                     int dig_type,
                     double symbol_rate,
                     double fsk_deviation,
                     int filter_type,
                     double filter_alpha,
                     int sps,
                     int filter_span,
                     int pn_order,
                     int pn_seed,
                     int symbols,
                     int dsss_code,
                     int ofdm_fft,
                     int ofdm_symbols,
                     int ofdm_guard_left,
                     int ofdm_guard_right,
                     int ofdm_cp,
                     int ofdm_window,
                     bool ofdm_null_dc,
                     int ofdm_mod,
                     double ofdm_idle,
                     double ofdm_rate)
    : gr::sync_block("harogic_sg_sink",
                     gr::io_signature::make(0, 1, sizeof(gr_complex)),
                     gr::io_signature::make(0, 0, 0)),
      d_scale(scale),
      d_max_latency(std::max(1.0, max_latency_ms) / 1000.0)
{
    check_rate(samp_rate);
    d_dev_settings.device_index = device_index;
    d_dev_settings.ext_ref = ext_ref;
    d_dev_settings.ref_freq = ref_freq;
    d_dev_settings.ref_out = ref_out;
    d_dev_settings.lo_mode = lo_mode;
    d_dev_settings.rf_port = rf_port;

    set_kind(mode);
    d_plan.frequency = frequency;
    d_plan.level = level;
    d_plan.samp_rate = samp_rate;
    d_plan.rf_on = rf_on;
    d_plan.freq_offset = freq_offset;
    d_plan.phase_offset = phase_offset;
    d_plan.sweep_start = sweep_start;
    d_plan.sweep_stop = sweep_stop;
    d_plan.sweep_step = sweep_step;
    d_plan.level_start = level_start;
    d_plan.level_stop = level_stop;
    d_plan.level_step = level_step;
    d_plan.dwell = dwell;
    d_plan.mod.mod_rate = mod_rate;
    d_plan.mod.mod_shape = mod_shape;
    d_plan.mod.am_depth = am_depth;
    d_plan.mod.fm_deviation = fm_deviation;
    d_plan.mod.pm_deviation = pm_deviation;
    d_plan.mod.pulse_width = pulse_width;
    d_plan.mod.pulse_period = pulse_period;
    d_plan.mod.tone_count = tone_count;
    d_plan.mod.tone_spacing = tone_spacing;
    d_plan.mod.tone_phase = tone_phase;
    d_plan.mod.ramp_span = ramp_span;
    d_plan.mod.ramp_time = ramp_time;
    d_plan.mod.ramp_period = ramp_period;
    d_plan.mod.noise_bw = noise_bw;
    d_plan.mod.noise_length = noise_length;
    d_plan.mod.dig_type = dig_type;
    d_plan.mod.symbol_rate = symbol_rate;
    d_plan.mod.fsk_deviation = fsk_deviation;
    d_plan.mod.filter_type = filter_type;
    d_plan.mod.filter_alpha = filter_alpha;
    d_plan.mod.sps = sps;
    d_plan.mod.filter_span = filter_span;
    d_plan.mod.pn_order = pn_order;
    d_plan.mod.pn_seed = pn_seed;
    d_plan.mod.symbols = symbols;
    d_plan.mod.dsss_code = dsss_code;
    d_plan.mod.ofdm_fft = ofdm_fft;
    d_plan.mod.ofdm_symbols = ofdm_symbols;
    d_plan.mod.ofdm_guard_left = ofdm_guard_left;
    d_plan.mod.ofdm_guard_right = ofdm_guard_right;
    d_plan.mod.ofdm_cp = ofdm_cp;
    d_plan.mod.ofdm_window = ofdm_window;
    d_plan.mod.ofdm_null_dc = ofdm_null_dc;
    d_plan.mod.ofdm_mod = ofdm_mod;
    d_plan.mod.ofdm_idle = ofdm_idle;
    d_plan.mod.ofdm_rate = ofdm_rate;

    set_output_multiple(1024);

    message_port_register_in(pmt::mp("cmd"));
    set_msg_handler(pmt::mp("cmd"), [this](const pmt::pmt_t& m) { handle_cmd(m); });

    // A block without connections (e.g. CW / sweep only) is left out of the
    // flowgraph and never gets start()/stop(), so open the generator now. The
    // built-in modes start transmitting right away; IQ mode arms in start().
    std::lock_guard<std::mutex> lock(d_mutex);
    d_dev = std::make_unique<h2_device>(d_dev_settings, d_logger);
    if (!streaming())
        d_dev->arm(d_plan);
}

sink_impl::~sink_impl()
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_dev.reset(); // RF off + close
}

void sink_impl::open_and_arm()
{
    // Caller holds d_mutex.
    if (!d_dev)
        d_dev = std::make_unique<h2_device>(d_dev_settings, d_logger);
    d_dev->arm(d_plan);
    d_anchor_valid = false;
    d_fill = 0;
}

bool sink_impl::start()
{
    std::lock_guard<std::mutex> lock(d_mutex);
    open_and_arm();
    return true;
}

bool sink_impl::stop()
{
    std::lock_guard<std::mutex> lock(d_mutex);
    if (d_dev) {
        d_dev->rf_off();
        d_dev.reset();
    }
    if (d_underruns)
        d_logger->info("{} underrun(s) during the run", d_underruns.load());
    return true;
}

void sink_impl::rearm()
{
    // Caller holds d_mutex. Re-arming flushes the API queue: restart pacing.
    if (d_dev) {
        d_dev->arm(d_plan);
        d_anchor_valid = false;
        d_fill = 0;
    }
}

size_t sink_impl::chunk_size() const
{
    // ~5 ms of samples, between 4k and 256k
    const double s = d_plan.samp_rate * 0.005;
    return (size_t)std::clamp(s, 4096.0, 262144.0);
}

int sink_impl::work(int noutput_items,
                    gr_vector_const_void_star& input_items,
                    gr_vector_void_star& output_items)
{
    if (input_items.empty())
        return noutput_items;
    const gr_complex* in = static_cast<const gr_complex*>(input_items[0]);
    std::unique_lock<std::mutex> lock(d_mutex);
    if (!d_dev)
        return noutput_items;

    // --- pacing: keep at most d_max_latency seconds queued in the API. In the
    // built-in modes the input is consumed at samp_rate and discarded. ---
    const auto now = clock::now();
    if (!d_anchor_valid) {
        d_t0 = now;
        d_sent = 0;
        d_anchor_valid = true;
    }
    const double elapsed = std::chrono::duration<double>(now - d_t0).count();
    const double queued = (double)d_sent / d_plan.samp_rate - elapsed;
    if (queued < -0.005) {
        if (streaming()) {
            // Upstream could not keep up: the generator drained its queue.
            d_underruns++;
            if (now - d_last_underrun_log > std::chrono::seconds(2)) {
                d_logger->warn("underrun: upstream is slower than {} S/s",
                               d_plan.samp_rate);
                d_last_underrun_log = now;
            }
        }
        d_t0 = now;
        d_sent = 0;
    } else if (queued > d_max_latency) {
        const double wait = queued - d_max_latency;
        lock.unlock(); // let callbacks run while we wait
        std::this_thread::sleep_for(std::chrono::duration<double>(std::min(wait, 0.1)));
        return 0; // GNU Radio calls us again with the same samples
    }

    const size_t n = (size_t)noutput_items;
    if (!streaming()) {
        d_sent += n;
        return noutput_items;
    }

    // --- convert to interleaved int16 into the staging buffer (VOLK, saturating) ---
    // The API is far faster with large transfers (8k samples: ~39 MS/s, 256k:
    // ~78 MS/s), so samples are collected into ~5 ms chunks before sending.
    const size_t chunk = chunk_size();
    if (d_buf.size() < 2 * (d_fill + n))
        d_buf.resize(2 * std::max(d_fill + n, chunk));
    volk_32f_s32f_convert_16i(d_buf.data() + 2 * d_fill,
                              reinterpret_cast<const float*>(in),
                              (float)(32767.0 * d_scale),
                              (unsigned int)(2 * n));
    d_fill += n;

    if (d_fill >= chunk) {
        int rc = d_dev->send_iq(d_buf.data(), (uint32_t)d_fill);
        if (rc == H2_WARNING_BUS_TIMEOUT)
            d_logger->warn("tx_send_stream: bus timeout");
        d_sent += d_fill;
        d_fill = 0;
    }
    return noutput_items;
}

void sink_impl::set_mode(int mode)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    set_kind(mode);
    rearm();
}

void sink_impl::set_frequency(double frequency)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.frequency = frequency;
    rearm();
}

void sink_impl::set_level(double level)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.level = level;
    rearm();
}

void sink_impl::set_samp_rate(double samp_rate)
{
    check_rate(samp_rate);
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.samp_rate = samp_rate;
    rearm();
}

void sink_impl::set_rf_on(bool rf_on)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.rf_on = rf_on;
    rearm();
}

void sink_impl::set_scale(double scale)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_scale = scale;
}

void sink_impl::set_sweep(double start, double stop, double step)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.sweep_start = start;
    d_plan.sweep_stop = stop;
    d_plan.sweep_step = step;
    if (d_plan.kind == tx_plan::FREQ_SWEEP)
        rearm();
}

void sink_impl::set_level_sweep(double start, double stop, double step)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.level_start = start;
    d_plan.level_stop = stop;
    d_plan.level_step = step;
    if (d_plan.kind == tx_plan::LEVEL_SWEEP)
        rearm();
}

void sink_impl::set_dwell(double dwell)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.dwell = dwell;
    if (d_plan.kind == tx_plan::FREQ_SWEEP || d_plan.kind == tx_plan::LEVEL_SWEEP)
        rearm();
}

void sink_impl::set_freq_offset(double freq_offset)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.freq_offset = freq_offset;
    if (streaming())
        rearm();
}

void sink_impl::set_phase_offset(double phase_offset)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_plan.phase_offset = phase_offset;
    if (streaming())
        rearm();
}

void sink_impl::set_lo_mode(int lo_mode)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_dev_settings.lo_mode = lo_mode;
    if (d_dev) {
        d_dev->set_lo_mode(lo_mode, d_plan); // re-arms
        d_anchor_valid = false;
        d_fill = 0;
    }
}

void sink_impl::set_max_latency_ms(double max_latency_ms)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_max_latency = std::max(1.0, max_latency_ms) / 1000.0;
}

template <typename F>
void sink_impl::update_mod(F&& change, std::initializer_list<mod_settings::wave_t> waves)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    change(d_plan.mod);
    if (d_plan.kind != tx_plan::PLAYBACK)
        return;
    for (auto w : waves)
        if (w == d_plan.mod.wave) {
            rearm(); // regenerates only if the waveform settings changed
            return;
        }
}

void sink_impl::set_analog_mod(
    double rate, int shape, double am_depth, double fm_deviation, double pm_deviation)
{
    update_mod(
        [&](mod_settings& m) {
            m.mod_rate = rate;
            m.mod_shape = shape;
            m.am_depth = am_depth;
            m.fm_deviation = fm_deviation;
            m.pm_deviation = pm_deviation;
        },
        { mod_settings::AM, mod_settings::FM, mod_settings::PM });
}

void sink_impl::set_pulse(double width, double period)
{
    update_mod(
        [&](mod_settings& m) {
            m.pulse_width = width;
            m.pulse_period = period;
        },
        { mod_settings::PULSE });
}

void sink_impl::set_multitone(int count, double spacing, int phase_mode)
{
    update_mod(
        [&](mod_settings& m) {
            m.tone_count = count;
            m.tone_spacing = spacing;
            m.tone_phase = phase_mode;
        },
        { mod_settings::MULTITONE });
}

void sink_impl::set_ramp_sweep(double span, double sweep_time, double period)
{
    update_mod(
        [&](mod_settings& m) {
            m.ramp_span = span;
            m.ramp_time = sweep_time;
            m.ramp_period = period;
        },
        { mod_settings::RAMP_SWEEP });
}

void sink_impl::set_noise(double bandwidth, double length)
{
    update_mod(
        [&](mod_settings& m) {
            m.noise_bw = bandwidth;
            m.noise_length = length;
        },
        { mod_settings::AWGN });
}

void sink_impl::set_digital_mod(int type,
                                double symbol_rate,
                                double fsk_deviation,
                                int filter_type,
                                double alpha,
                                int sps,
                                int filter_span,
                                int pn_order,
                                int pn_seed,
                                int symbols)
{
    update_mod(
        [&](mod_settings& m) {
            m.dig_type = type;
            m.symbol_rate = symbol_rate;
            m.fsk_deviation = fsk_deviation;
            m.filter_type = filter_type;
            m.filter_alpha = alpha;
            m.sps = sps;
            m.filter_span = filter_span;
            m.pn_order = pn_order;
            m.pn_seed = pn_seed;
            m.symbols = symbols;
        },
        { mod_settings::DIGITAL, mod_settings::DSSS });
}

void sink_impl::set_dsss_code(int code)
{
    update_mod([&](mod_settings& m) { m.dsss_code = code; }, { mod_settings::DSSS });
}

void sink_impl::set_ofdm(int fft_size,
                         int symbols,
                         int guard_left,
                         int guard_right,
                         int cp_percent,
                         int window_percent,
                         bool null_dc,
                         int subcarrier_mod,
                         double idle_time,
                         double samp_rate)
{
    update_mod(
        [&](mod_settings& m) {
            m.ofdm_fft = fft_size;
            m.ofdm_symbols = symbols;
            m.ofdm_guard_left = guard_left;
            m.ofdm_guard_right = guard_right;
            m.ofdm_cp = cp_percent;
            m.ofdm_window = window_percent;
            m.ofdm_null_dc = null_dc;
            m.ofdm_mod = subcarrier_mod;
            m.ofdm_idle = idle_time;
            m.ofdm_rate = samp_rate;
        },
        { mod_settings::OFDM });
}

void sink_impl::handle_cmd(const pmt::pmt_t& msg)
{
    if (!pmt::is_dict(msg)) {
        d_logger->warn("cmd: expected a dict, e.g. {{'freq': 868.3e6, 'level': -20}}");
        return;
    }
    auto get = [&](const char* key) {
        return pmt::dict_ref(msg, pmt::mp(key), pmt::PMT_NIL);
    };
    std::lock_guard<std::mutex> lock(d_mutex);
    bool changed = false;
    auto num = [&](const char* key, double& dst) {
        pmt::pmt_t x = get(key);
        if (!pmt::is_null(x)) {
            dst = pmt::to_double(x);
            changed = true;
        }
    };
    pmt::pmt_t v;
    auto integer = [&](const char* key, int& dst) {
        pmt::pmt_t x = get(key);
        if (!pmt::is_null(x)) {
            dst = pmt::is_integer(x) ? (int)pmt::to_long(x) : (int)pmt::to_double(x);
            changed = true;
        }
    };
    auto boolean = [&](const char* key, bool& dst) {
        pmt::pmt_t x = get(key);
        if (!pmt::is_null(x)) {
            dst = pmt::is_bool(x) ? pmt::to_bool(x) : pmt::to_long(x) != 0;
            changed = true;
        }
    };
    if (!pmt::is_null(v = get("mode"))) {
        set_kind((int)pmt::to_long(v));
        changed = true;
    }
    num("freq", d_plan.frequency);
    num("level", d_plan.level);
    num("freq_offset", d_plan.freq_offset);
    num("phase_offset", d_plan.phase_offset);
    num("sweep_start", d_plan.sweep_start);
    num("sweep_stop", d_plan.sweep_stop);
    num("sweep_step", d_plan.sweep_step);
    num("level_start", d_plan.level_start);
    num("level_stop", d_plan.level_stop);
    num("level_step", d_plan.level_step);
    num("dwell", d_plan.dwell);
    num("mod_rate", d_plan.mod.mod_rate);
    integer("mod_shape", d_plan.mod.mod_shape);
    num("am_depth", d_plan.mod.am_depth);
    num("fm_deviation", d_plan.mod.fm_deviation);
    num("pm_deviation", d_plan.mod.pm_deviation);
    num("pulse_width", d_plan.mod.pulse_width);
    num("pulse_period", d_plan.mod.pulse_period);
    integer("tone_count", d_plan.mod.tone_count);
    num("tone_spacing", d_plan.mod.tone_spacing);
    integer("tone_phase", d_plan.mod.tone_phase);
    num("ramp_span", d_plan.mod.ramp_span);
    num("ramp_time", d_plan.mod.ramp_time);
    num("ramp_period", d_plan.mod.ramp_period);
    num("noise_bw", d_plan.mod.noise_bw);
    num("noise_length", d_plan.mod.noise_length);
    integer("dig_type", d_plan.mod.dig_type);
    num("symbol_rate", d_plan.mod.symbol_rate);
    num("fsk_deviation", d_plan.mod.fsk_deviation);
    integer("filter_type", d_plan.mod.filter_type);
    num("filter_alpha", d_plan.mod.filter_alpha);
    integer("sps", d_plan.mod.sps);
    integer("filter_span", d_plan.mod.filter_span);
    integer("pn_order", d_plan.mod.pn_order);
    integer("pn_seed", d_plan.mod.pn_seed);
    integer("symbols", d_plan.mod.symbols);
    integer("dsss_code", d_plan.mod.dsss_code);
    integer("ofdm_fft", d_plan.mod.ofdm_fft);
    integer("ofdm_symbols", d_plan.mod.ofdm_symbols);
    integer("ofdm_guard_left", d_plan.mod.ofdm_guard_left);
    integer("ofdm_guard_right", d_plan.mod.ofdm_guard_right);
    integer("ofdm_cp", d_plan.mod.ofdm_cp);
    integer("ofdm_window", d_plan.mod.ofdm_window);
    boolean("ofdm_null_dc", d_plan.mod.ofdm_null_dc);
    integer("ofdm_mod", d_plan.mod.ofdm_mod);
    num("ofdm_idle", d_plan.mod.ofdm_idle);
    num("ofdm_rate", d_plan.mod.ofdm_rate);
    boolean("rf", d_plan.rf_on);
    if (!pmt::is_null(v = get("samp_rate"))) {
        double r = pmt::to_double(v);
        check_rate(r);
        d_plan.samp_rate = r;
        changed = true;
    }
    if (!pmt::is_null(v = get("lo_mode"))) {
        d_dev_settings.lo_mode = (int)pmt::to_long(v);
        if (d_dev) {
            d_dev->set_lo_mode(d_dev_settings.lo_mode, d_plan); // re-arms
            d_anchor_valid = false;
            d_fill = 0;
            changed = false;
        }
    }
    if (changed)
        rearm();
}

} // namespace harogic_sg
} // namespace gr
