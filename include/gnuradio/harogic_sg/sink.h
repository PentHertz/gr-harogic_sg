/* -*- c++ -*- */
/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_HAROGIC_SG_SINK_H
#define INCLUDED_HAROGIC_SG_SINK_H

#include <gnuradio/harogic_sg/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace harogic_sg {

/*!
 * \brief Harogic H2 signal generator sink.
 * \ingroup harogic_sg
 *
 * \details
 * One block for everything the generator can transmit, selected by \p mode
 * (changeable while running):
 *  - 0 GNU Radio IQ: streams the complex input in real time (TX_REALTIME),
 *        195.3125 kS/s .. 62.5 MS/s. The block paces itself against
 *        \p samp_rate (the H2 API queues without back-pressure), keeping at
 *        most \p max_latency_ms queued; no Throttle is needed.
 *  - 1 CW: carrier at \p frequency / \p level.
 *  - 2 Frequency sweep (FSCAN): \p sweep_start .. \p sweep_stop in
 *        \p sweep_step, \p dwell s per point, repeated continuously.
 *  - 3 Level sweep (LSCAN): at \p frequency, \p level_start .. \p level_stop in
 *        \p level_step dB, \p dwell s per point.
 *
 * The input is optional. In modes 1-3 a connected input is consumed at
 * \p samp_rate and discarded.
 *
 * The "cmd" message port accepts a dict with any of: mode, freq, level,
 * rf (bool), samp_rate, freq_offset, phase_offset, lo_mode, sweep_start,
 * sweep_stop, sweep_step, level_start, level_stop, level_step, dwell.
 *
 * The generator is opened when the block is created (a block without
 * connections never gets start()/stop()). RF is switched off on stop(), on
 * destruction and at process exit.
 */
class HAROGIC_SG_API sink : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<sink> sptr;

    /*!
     * \param device_index   USB device number (0 = first generator)
     * \param mode           0 GNU Radio IQ, 1 CW, 2 frequency sweep, 3 level sweep
     * \param frequency      RF frequency in Hz (IQ, CW, level sweep)
     * \param level          output level in dBm (IQ, CW, frequency sweep)
     * \param samp_rate      IQ sample rate in S/s (195.3125e3 .. 62.5e6)
     * \param rf_on          RF output enabled
     * \param scale          IQ multiplier before int16 conversion (1.0 = full scale)
     * \param sweep_start    frequency sweep start in Hz
     * \param sweep_stop     frequency sweep stop in Hz
     * \param sweep_step     frequency sweep step in Hz
     * \param level_start    level sweep start in dBm
     * \param level_stop     level sweep stop in dBm
     * \param level_step     level sweep step in dB
     * \param dwell          sweep dwell time per point in s
     * \param freq_offset    digital frequency offset in Hz (IQ)
     * \param phase_offset   phase offset in radians (IQ)
     * \param lo_mode        0 auto, 1 high speed, 2 low spur, 3 low phase noise
     * \param rf_port        0 standard, 1 high power (only with that option)
     * \param ext_ref        use an external reference clock
     * \param ref_freq       external reference frequency in Hz
     * \param ref_out        enable the reference clock output
     * \param max_latency_ms maximum IQ queued in the API, in ms
     */
    static sptr make(int device_index = 0,
                     int mode = 0,
                     double frequency = 868.3e6,
                     double level = -30.0,
                     double samp_rate = 1e6,
                     bool rf_on = true,
                     double scale = 1.0,
                     double sweep_start = 868.0e6,
                     double sweep_stop = 868.6e6,
                     double sweep_step = 100e3,
                     double level_start = -40.0,
                     double level_stop = -10.0,
                     double level_step = 1.0,
                     double dwell = 0.1,
                     double freq_offset = 0.0,
                     double phase_offset = 0.0,
                     int lo_mode = 0,
                     int rf_port = 0,
                     bool ext_ref = false,
                     double ref_freq = 10e6,
                     bool ref_out = false,
                     double max_latency_ms = 50.0);

    virtual void set_mode(int mode) = 0;
    virtual void set_frequency(double frequency) = 0;
    virtual void set_level(double level) = 0;
    virtual void set_samp_rate(double samp_rate) = 0;
    virtual void set_rf_on(bool rf_on) = 0;
    virtual void set_scale(double scale) = 0;
    virtual void set_sweep(double start, double stop, double step) = 0;
    virtual void set_level_sweep(double start, double stop, double step) = 0;
    virtual void set_dwell(double dwell) = 0;
    virtual void set_freq_offset(double freq_offset) = 0;
    virtual void set_phase_offset(double phase_offset) = 0;
    virtual void set_lo_mode(int lo_mode) = 0;
    virtual void set_max_latency_ms(double max_latency_ms) = 0;

    virtual int mode() const = 0;
    virtual double frequency() const = 0;
    virtual double level() const = 0;
    virtual double samp_rate() const = 0;
    virtual bool rf_on() const = 0;
    /*! Number of times the generator ran out of IQ (upstream too slow). */
    virtual uint64_t underruns() const = 0;
};

} // namespace harogic_sg
} // namespace gr

#endif /* INCLUDED_HAROGIC_SG_SINK_H */
