/* -*- c++ -*- */
/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Thin RAII wrapper around the Harogic H2 API (libh2api) for one TX channel.
 *
 * Arming sequence (matches what SGStudio sends; without channel_config_trigger
 * the API accepts everything but the RF output stays silent):
 *   channel_stop -> tx_config_ffm / tx_config_fscan / tx_config_lscan
 *   -> tx_config_stream -> channel_config_trigger -> tx_config_output
 *   -> channel_start -> channel_bus_trigger
 * Any change made while running (frequency, level, rate...) needs a full re-arm,
 * otherwise the output goes quiet.
 */

#ifndef INCLUDED_HAROGIC_SG_H2_DEVICE_H
#define INCLUDED_HAROGIC_SG_H2_DEVICE_H

#include <gnuradio/logger.h>
#include <h2_api.h>
#include <mutex>
#include <string>

namespace gr {
namespace harogic_sg {

struct device_settings {
    int device_index = 0;
    bool ext_ref = false;
    double ref_freq = 10e6; // only used with an external reference
    bool ref_out = false;
    int lo_mode = LOMODE_AUTO;
    int rf_port = 0; // 0 = standard, 1 = high power (only on units with that option)
};

/* What the TX channel should do when armed. */
struct tx_plan {
    enum kind_t { CW = 0, FREQ_SWEEP = 1, LEVEL_SWEEP = 2, REALTIME = 3 };
    kind_t kind = CW;
    double frequency = 868.3e6;
    double level = -30.0;
    bool rf_on = true;
    // realtime IQ
    double samp_rate = 1e6;
    double freq_offset = 0.0;
    double phase_offset = 0.0;
    // frequency sweep (FSCAN)
    double sweep_start = 868.0e6;
    double sweep_stop = 868.6e6;
    double sweep_step = 100e3;
    // level sweep (LSCAN)
    double level_start = -40.0;
    double level_stop = -10.0;
    double level_step = 1.0;
    double dwell = 0.1; // seconds per sweep point
};

class h2_device
{
public:
    h2_device(const device_settings& s, gr::logger_ptr log);
    ~h2_device();

    h2_device(const h2_device&) = delete;
    h2_device& operator=(const h2_device&) = delete;

    /* Stop the channel, apply the plan and start transmitting (if rf_on). */
    void arm(const tx_plan& p);
    /* Change only the LO mode (re-arms with the given plan). */
    void set_lo_mode(int mode, const tx_plan& p);
    /* RF output off + channel stop. Never throws. */
    void rf_off() noexcept;
    /* RF off + close without logging or blocking; used by the atexit handler. */
    void emergency_shutdown() noexcept;

    /* Returns H2_NOERROR or a positive warning code; throws on errors. */
    int send_iq(int16_t* iq, uint32_t points);

    const device_info& info() const { return d_info; }
    std::string describe() const;

private:
    int check(int rc, const char* what, bool fatal = true);
    void open(const device_settings& s);

    gr::logger_ptr d_logger;
    std::mutex d_mutex;
    void* d_dev = nullptr;
    channel d_ch[MAXCHANNELS];
    int d_tx = -1;
    device_info d_info;
    device_settings d_settings;
    tx_stream d_stream;
    stream_trigger d_trigger;
};

std::string h2_error_string(int rc);

} // namespace harogic_sg
} // namespace gr

#endif /* INCLUDED_HAROGIC_SG_H2_DEVICE_H */
