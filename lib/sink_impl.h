/* -*- c++ -*- */
/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_HAROGIC_SG_SINK_IMPL_H
#define INCLUDED_HAROGIC_SG_SINK_IMPL_H

#include "h2_device.h"
#include <gnuradio/harogic_sg/sink.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace gr {
namespace harogic_sg {

class sink_impl : public sink
{
private:
    device_settings d_dev_settings;
    tx_plan d_plan;
    double d_scale;
    double d_max_latency;
    std::unique_ptr<h2_device> d_dev;
    std::mutex d_mutex;         // serializes work() and re-arming
    std::vector<int16_t> d_buf; // staging buffer, interleaved int16 I/Q
    size_t d_fill = 0;          // samples currently staged

    using clock = std::chrono::steady_clock;
    clock::time_point d_t0;
    uint64_t d_sent = 0;
    bool d_anchor_valid = false;
    std::atomic<uint64_t> d_underruns{ 0 };
    clock::time_point d_last_underrun_log;

    bool streaming() const { return d_plan.kind == tx_plan::REALTIME; }
    void open_and_arm();
    void rearm();
    size_t chunk_size() const;
    void handle_cmd(const pmt::pmt_t& msg);
    static tx_plan::kind_t to_kind(int mode);

public:
    sink_impl(int device_index,
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
              double max_latency_ms);
    ~sink_impl() override;

    bool start() override;
    bool stop() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;

    void set_mode(int mode) override;
    void set_frequency(double frequency) override;
    void set_level(double level) override;
    void set_samp_rate(double samp_rate) override;
    void set_rf_on(bool rf_on) override;
    void set_scale(double scale) override;
    void set_sweep(double start, double stop, double step) override;
    void set_level_sweep(double start, double stop, double step) override;
    void set_dwell(double dwell) override;
    void set_freq_offset(double freq_offset) override;
    void set_phase_offset(double phase_offset) override;
    void set_lo_mode(int lo_mode) override;
    void set_max_latency_ms(double max_latency_ms) override;

    int mode() const override;
    double frequency() const override { return d_plan.frequency; }
    double level() const override { return d_plan.level; }
    double samp_rate() const override { return d_plan.samp_rate; }
    bool rf_on() const override { return d_plan.rf_on; }
    uint64_t underruns() const override { return d_underruns.load(); }
};

} // namespace harogic_sg
} // namespace gr

#endif /* INCLUDED_HAROGIC_SG_SINK_IMPL_H */
