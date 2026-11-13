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
#include <initializer_list>
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
    mutable std::mutex d_mutex; // serializes work() and re-arming
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
    void set_kind(int mode);
    /* Apply a change to the built-in settings; re-arm if it affects what plays. */
    template <typename F>
    void update_mod(F&& change, std::initializer_list<mod_settings::wave_t> waves);

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
              double ofdm_rate);
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

    void set_analog_mod(double rate,
                        int shape,
                        double am_depth,
                        double fm_deviation,
                        double pm_deviation) override;
    void set_pulse(double width, double period) override;
    void set_multitone(int count, double spacing, int phase_mode) override;
    void set_ramp_sweep(double span, double sweep_time, double period) override;
    void set_noise(double bandwidth, double length) override;
    void set_digital_mod(int type,
                         double symbol_rate,
                         double fsk_deviation,
                         int filter_type,
                         double alpha,
                         int sps,
                         int filter_span,
                         int pn_order,
                         int pn_seed,
                         int symbols) override;
    void set_dsss_code(int code) override;
    void set_ofdm(int fft_size,
                  int symbols,
                  int guard_left,
                  int guard_right,
                  int cp_percent,
                  int window_percent,
                  bool null_dc,
                  int subcarrier_mod,
                  double idle_time,
                  double samp_rate) override;

    int mode() const override;
    double frequency() const override { return d_plan.frequency; }
    double level() const override { return d_plan.level; }
    double samp_rate() const override { return d_plan.samp_rate; }
    bool rf_on() const override { return d_plan.rf_on; }
    uint64_t underruns() const override { return d_underruns.load(); }
    double playback_rate() const override;
    uint64_t playback_points() const override;
};

} // namespace harogic_sg
} // namespace gr

#endif /* INCLUDED_HAROGIC_SG_SINK_IMPL_H */
