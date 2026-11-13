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
                      double max_latency_ms)
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
                                                max_latency_ms);
}

static void check_rate(double r)
{
    if (r < MIN_RATE - 1 || r > MAX_RATE + 1)
        throw std::invalid_argument(
            "harogic_sg.sink: samp_rate must be between 195312.5 and 62.5e6 S/s");
}

tx_plan::kind_t sink_impl::to_kind(int mode)
{
    switch (mode) {
    case 0:
        return tx_plan::REALTIME;
    case 1:
        return tx_plan::CW;
    case 2:
        return tx_plan::FREQ_SWEEP;
    case 3:
        return tx_plan::LEVEL_SWEEP;
    default:
        throw std::invalid_argument("harogic_sg.sink: mode must be 0 (GNU Radio IQ), "
                                    "1 (CW), 2 (frequency sweep) or 3 (level sweep)");
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
    default:
        return 3;
    }
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
                     double max_latency_ms)
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

    d_plan.kind = to_kind(mode);
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
    d_plan.kind = to_kind(mode);
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
    if (!pmt::is_null(v = get("mode"))) {
        d_plan.kind = to_kind((int)pmt::to_long(v));
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
    if (!pmt::is_null(v = get("rf"))) {
        d_plan.rf_on = pmt::is_bool(v) ? pmt::to_bool(v) : pmt::to_long(v) != 0;
        changed = true;
    }
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
