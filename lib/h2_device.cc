/* -*- c++ -*- */
/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "h2_device.h"
#include <h2_mod.h>
#include <limits.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gr {
namespace harogic_sg {

/*
 * Safety net: the generator keeps transmitting when a program exits without
 * switching RF off (SGStudio itself does this). A block that is not connected
 * to anything is never stopped by GNU Radio and Python does not always destroy
 * it at shutdown, so every open device is registered here and an atexit()
 * handler switches RF off and closes whatever is still open.
 */
namespace {
std::mutex& registry_mutex()
{
    static std::mutex m;
    return m;
}
std::set<h2_device*>& registry()
{
    static std::set<h2_device*> s;
    return s;
}
void rf_off_at_exit()
{
    std::lock_guard<std::mutex> lock(registry_mutex());
    for (h2_device* d : registry())
        d->emergency_shutdown();
    registry().clear();
}
void register_device(h2_device* d)
{
    registry(); // construct before registering the handler (destruction order)
    static bool installed = (std::atexit(rf_off_at_exit) == 0);
    (void)installed;
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry().insert(d);
}
void unregister_device(h2_device* d)
{
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry().erase(d);
}
} // namespace

std::string h2_error_string(int rc)
{
    char desc[512] = { 0 };
    char sugg[512] = { 0 };
    device_query_discription_retval(rc, H2_LANGUAGE_ENGLISH, desc, sugg);
    std::ostringstream os;
    os << rc;
    if (desc[0])
        os << " (" << desc << (sugg[0] ? " - " : "") << sugg << ")";
    return os.str();
}

const char* mod_settings::name(wave_t w)
{
    static const char* names[NUM_WAVES] = { "AM",        "FM",         "PM",
                                            "Pulse",     "Multitone",  "Ramp Sweep",
                                            "AWGN",      "Digital Mod", "DSSS",
                                            "OFDM" };
    return (w >= 0 && w < NUM_WAVES) ? names[w] : "?";
}

std::string mod_settings::key() const
{
    std::ostringstream os;
    os.precision(17);
    os << (int)wave << ':';
    switch (wave) {
    case AM:
        os << mod_rate << ',' << mod_shape << ',' << am_depth;
        break;
    case FM:
        os << mod_rate << ',' << mod_shape << ',' << fm_deviation;
        break;
    case PM:
        os << mod_rate << ',' << mod_shape << ',' << pm_deviation;
        break;
    case PULSE:
        os << pulse_width << ',' << pulse_period;
        break;
    case MULTITONE:
        os << tone_count << ',' << tone_spacing << ',' << tone_phase;
        break;
    case RAMP_SWEEP:
        os << ramp_span << ',' << ramp_time << ',' << ramp_period;
        break;
    case AWGN:
        os << noise_bw << ',' << noise_length;
        break;
    case DSSS:
        os << dsss_code << ',';
        [[fallthrough]];
    case DIGITAL:
        os << dig_type << ',' << symbol_rate << ',' << fsk_deviation << ','
           << filter_type << ',' << filter_alpha << ',' << sps << ',' << filter_span
           << ',' << pn_order << ',' << pn_seed << ',' << symbols;
        break;
    case OFDM:
        os << ofdm_fft << ',' << ofdm_symbols << ',' << ofdm_guard_left << ','
           << ofdm_guard_right << ',' << ofdm_cp << ',' << ofdm_window << ','
           << ofdm_null_dc << ',' << ofdm_mod << ',' << ofdm_idle << ',' << ofdm_rate;
        break;
    default:
        break;
    }
    return os.str();
}

int h2_device::check(int rc, const char* what, bool fatal)
{
    if (rc < 0) {
        std::string msg = std::string(what) + " failed: " + h2_error_string(rc);
        if (fatal)
            throw std::runtime_error("harogic_sg: " + msg);
        d_logger->error("{}", msg);
    } else if (rc > 0) {
        d_logger->warn("{}: {}", what, h2_error_string(rc));
    }
    return rc;
}

static std::string exe_dir()
{
    char buf[PATH_MAX] = { 0 };
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return ".";
    std::string p(buf, n);
    return p.substr(0, p.find_last_of('/'));
}

h2_device::h2_device(const device_settings& s, gr::logger_ptr log)
    : d_logger(log), d_settings(s)
{
    std::memset(d_ch, 0, sizeof(d_ch));
    std::memset(&d_info, 0, sizeof(d_info));
    std::memset(&d_stream, 0, sizeof(d_stream));
    std::memset(&d_trigger, 0, sizeof(d_trigger));
    open(s);
    register_device(this);
}

void h2_device::open(const device_settings& s)
{
    std::vector<device_info> list(16);
    uint32_t count = 0;
    check(device_list_usb(list.data(), &count), "device_list_usb");
    if (count == 0)
        throw std::runtime_error(
            "harogic_sg: no Harogic H2 signal generator found on USB. Check the cable, "
            "that /etc/h2usb_enum.conf lists your VID:PID and that no other program "
            "(SGStudio) has the device open.");
    if (s.device_index < 0 || (uint32_t)s.device_index >= count)
        throw std::runtime_error("harogic_sg: device_index " +
                                 std::to_string(s.device_index) + " out of range (" +
                                 std::to_string(count) + " device(s) found)");

    const uint16_t chs = list[s.device_index].chn ? list[s.device_index].chn : 1;
    check(device_open_usb((uint8_t)s.device_index, chs, &d_dev, d_ch, &d_info),
          "device_open_usb");

    for (int i = 0; i < d_info.chn && i < MAXCHANNELS; i++) {
        if (d_ch[i].type == CHANNELTYPE_TX) {
            d_tx = i;
            break;
        }
    }
    if (d_tx < 0) {
        device_close(&d_dev);
        throw std::runtime_error("harogic_sg: device has no TX channel");
    }

    d_logger->info("{}", describe());

    // libh2api reads <directory of the running executable>/CalFile/<model>.ini.
    // For GNU Radio that is usually /usr/bin/CalFile (python3).
    std::string cal = exe_dir() + "/CalFile/" + std::to_string(d_info.model) + ".ini";
    if (!std::ifstream(cal).good())
        d_logger->warn("calibration file {} not found: output level accuracy is "
                       "degraded. Copy it from SGStudio's bin/CalFile/.",
                       cal);

    check(device_config_power_state(&d_dev, POWER_ON), "device_config_power_state",
          false);
    // SGStudio uses 100 MHz for the internal reference.
    check(device_config_clock(&d_dev,
                              s.ext_ref ? REF_EXT : REF_INT,
                              s.ext_ref ? s.ref_freq : 100e6,
                              s.ref_out ? REF_OUT_ON : REF_OUT_OFF),
          "device_config_clock");
    check(channel_config_lo_mode(&d_ch[d_tx], (uint8_t)s.lo_mode),
          "channel_config_lo_mode");
    // Only touch the port when asked: selecting a port the unit does not have
    // crashed the API during testing.
    if (s.rf_port != 0)
        check(channel_config_port(&d_ch[d_tx], (uint16_t)s.rf_port),
              "channel_config_port");
    else
        check(channel_config_port(&d_ch[d_tx], 0), "channel_config_port", false);

    std::memset(&d_caps, 0, sizeof(d_caps));
    if (channel_tx_query_capabilities(&d_ch[d_tx], &d_caps) == H2_NOERROR) {
        d_caps_valid = true;
        d_logger->debug("TX capabilities: {:g}..{:g} Hz, {:g}..{:g} dBm, real-time "
                        "{:g}..{:g} S/s, playback {:g}..{:g} S/s, waveform memory {} "
                        "bytes",
                        d_caps.freq_min,
                        d_caps.freq_max,
                        d_caps.level_min,
                        d_caps.level_max,
                        d_caps.realtime_srate_min,
                        d_caps.realtime_srate_max,
                        d_caps.playback_srate_min,
                        d_caps.playback_srate_max,
                        (unsigned long long)d_caps.memory_size);
    }
}

h2_device::~h2_device()
{
    unregister_device(this);
    rf_off();
    if (d_dev) {
        std::lock_guard<std::mutex> lock(d_mutex);
        if (d_mod_state == 1)
            mod_close(&d_dev);
        device_close(&d_dev);
        d_dev = nullptr;
    }
}

std::string h2_device::describe() const
{
    std::ostringstream os;
    os << "Harogic H2 model " << d_info.model;
    if (d_info.model_name[0])
        os << " (" << d_info.model_name << ")";
    os << ", " << (int)d_info.chn << " channel(s)";
    for (int i = 0; i < d_info.versions && i < 16; i++)
        os << ", " << d_info.version_name[i];
    for (int i = 0; i < d_info.interfaces && i < 8; i++)
        os << ", " << d_info.interface_name[i];
    return os.str();
}

void h2_device::ensure_mod_open(const mod_settings& m)
{
    // Caller holds d_mutex. Only the waveforms backed by libgenSignalWave need
    // mod_open (license check); retry so that a license copied while running
    // is picked up.
    if (d_mod_state == 1 || !mod_settings::needs_license(m.wave))
        return;
    char version[50] = { 0 };
    int rc = mod_open(&d_dev, version);
    if (rc == H2_NOERROR) {
        d_mod_state = 1;
        d_logger->info("H2 modulation library {}", version);
        return;
    }
    d_mod_state = -1;
    std::ostringstream os;
    os << "harogic_sg: " << mod_settings::name(m.wave)
       << " needs the H2 modulation library, mod_open failed: " << h2_error_string(rc);
    if (rc == H2_ERROR_MOD_LICENSE_FILE_MISSING || rc == H2_ERROR_MOD_LICENSE_VERIFY_FAILED)
        os << ". Copy the license file " << d_info.model
           << "_<serial>_mod.lic from SGStudio's bin/CalFile/ to " << exe_dir()
           << "/CalFile/.";
    else if (rc == H2_ERROR_MOD_LIBRARY_MISSING)
        os << ". libgenSignalWave.so must be in the same directory as libh2api.so.";
    throw std::runtime_error(os.str());
}

namespace {
/* Warn when the modulation library clamped a parameter. */
struct clamp_report {
    gr::logger_ptr log;
    const char* wave;
    void operator()(const char* what, double requested, double applied) const
    {
        if (std::abs(requested - applied) > 1e-9 * std::max(1.0, std::abs(requested)))
            log->warn("{}: {} {:g} out of range, using {:g}", wave, what, requested, applied);
    }
};
} // namespace

void h2_device::generate(const mod_settings& m)
{
    // Caller holds d_mutex.
    const std::string key = m.key();
    if (key == d_gen_key && !d_wave.empty())
        return;

    ensure_mod_open(m);
    void** dev = &d_dev;
    const clamp_report adj{ d_logger, mod_settings::name(m.wave) };
    auto cfg = [&](int rc, const char* what) {
        if (rc == H2_ERROR_MOD_PROFILE_SET_FAILED)
            d_logger->warn("{}: parameters rejected, the previous valid ones are used",
                           what);
        else
            check(rc, what);
    };
    int32_t symbol_length = 0;

    switch (m.wave) {
    case mod_settings::AM: {
        am_profile in, out;
        mod_init_am(dev, &in);
        in.rate = m.mod_rate;
        in.depth = m.am_depth;
        in.shape = (uint8_t)m.mod_shape;
        out = in;
        cfg(mod_config_am(dev, &in, &out), "mod_config_am");
        adj("rate", in.rate, out.rate);
        adj("depth", in.depth, out.depth);
        adj("shape", in.shape, out.shape);
        break;
    }
    case mod_settings::FM: {
        fm_profile in, out;
        mod_init_fm(dev, &in);
        in.rate = m.mod_rate;
        in.deviation = m.fm_deviation;
        in.shape = (uint8_t)m.mod_shape;
        out = in;
        cfg(mod_config_fm(dev, &in, &out), "mod_config_fm");
        adj("rate", in.rate, out.rate);
        adj("deviation", in.deviation, out.deviation);
        adj("shape", in.shape, out.shape);
        break;
    }
    case mod_settings::PM: {
        pm_profile in, out;
        mod_init_pm(dev, &in);
        in.rate = m.mod_rate;
        in.phase_deviation = m.pm_deviation;
        in.init_phase = 0.0;
        in.shape = (uint8_t)m.mod_shape;
        out = in;
        cfg(mod_config_pm(dev, &in, &out), "mod_config_pm");
        adj("rate", in.rate, out.rate);
        adj("phase deviation", in.phase_deviation, out.phase_deviation);
        adj("shape", in.shape, out.shape);
        break;
    }
    case mod_settings::PULSE: {
        pulse_profile in, out;
        mod_init_pulse(dev, &in);
        in.width = m.pulse_width;
        in.period = m.pulse_period;
        out = in;
        cfg(mod_config_pulse(dev, &in, &out), "mod_config_pulse");
        adj("width", in.width, out.width);
        adj("period", in.period, out.period);
        break;
    }
    case mod_settings::MULTITONE: {
        multitone_profile in, out;
        mod_init_multitone(dev, &in);
        in.phase_mode = (uint8_t)m.tone_phase;
        in.seed = 1;
        in.tonecount = m.tone_count;
        in.freqspacing = m.tone_spacing;
        in.generate_tones = m.tone_count;
        in.generate_tone_idx = nullptr;
        out = in;
        cfg(mod_config_multitone(dev, &in, &out), "mod_config_multitone");
        adj("tone count", in.tonecount, out.tonecount);
        adj("tone spacing", in.freqspacing, out.freqspacing);
        adj("phase mode", in.phase_mode, out.phase_mode);
        break;
    }
    case mod_settings::RAMP_SWEEP: {
        rampsweep_profile in, out;
        mod_init_rampsweep(dev, &in);
        in.span = m.ramp_span;
        in.sweeptime = m.ramp_time;
        in.period = m.ramp_period;
        out = in;
        cfg(mod_config_rampsweep(dev, &in, &out), "mod_config_rampsweep");
        adj("span", in.span, out.span);
        adj("sweep time", in.sweeptime, out.sweeptime);
        adj("period", in.period, out.period);
        break;
    }
    case mod_settings::AWGN: {
        awgn_profile in, out;
        mod_init_awgn(dev, &in);
        in.bandwidth = m.noise_bw;
        in.length = m.noise_length;
        out = in;
        cfg(mod_config_awgn(dev, &in, &out), "mod_config_awgn");
        adj("bandwidth", in.bandwidth, out.bandwidth);
        adj("length", in.length, out.length);
        break;
    }
    case mod_settings::DIGITAL: {
        digitalmod_profile in, out;
        mod_init_digitalmod(dev, &in);
        in.digitalmod_type = (uint8_t)m.dig_type;
        in.symbolrate = m.symbol_rate;
        in.fskdeviation = m.fsk_deviation;
        in.pn = m.pn_order;
        in.sequenceseed = m.pn_seed;
        in.filter_type = (uint8_t)m.filter_type;
        in.filter_symbols = m.filter_span;
        in.sps = m.sps;
        in.alpha = m.filter_alpha;
        out = in;
        cfg(mod_config_digitalmod(dev, &in, &out), "mod_config_digitalmod");
        adj("modulation", in.digitalmod_type, out.digitalmod_type);
        adj("symbol rate", in.symbolrate, out.symbolrate);
        adj("FSK deviation", in.fskdeviation, out.fskdeviation);
        adj("PN order", in.pn, out.pn);
        adj("PN seed", in.sequenceseed, out.sequenceseed);
        adj("filter", in.filter_type, out.filter_type);
        adj("filter span", in.filter_symbols, out.filter_symbols);
        adj("samples per symbol", in.sps, out.sps);
        adj("alpha", in.alpha, out.alpha);
        symbol_length = m.symbols;
        if (symbol_length <= 0) // one PN period loops seamlessly
            symbol_length = (int32_t)((1u << std::clamp(out.pn, 4, 16)) - 1);
        break;
    }
    case mod_settings::DSSS: {
        dsss_profile in, out;
        mod_init_dsss(dev, &in);
        in.symbolrate = m.symbol_rate;
        in.digitalmod_type = (uint8_t)m.dig_type;
        in.code = m.dsss_code;
        in.filter_symbols = m.filter_span;
        in.sps = m.sps;
        in.alpha = m.filter_alpha;
        in.sequenceseed = m.pn_seed;
        in.filter_type = (uint8_t)m.filter_type;
        out = in;
        cfg(mod_config_dsss(dev, &in, &out), "mod_config_dsss");
        adj("symbol rate", in.symbolrate, out.symbolrate);
        adj("modulation", in.digitalmod_type, out.digitalmod_type);
        adj("spreading code order", in.code, out.code);
        adj("filter span", in.filter_symbols, out.filter_symbols);
        adj("samples per symbol", in.sps, out.sps);
        adj("alpha", in.alpha, out.alpha);
        adj("PN seed", in.sequenceseed, out.sequenceseed);
        adj("filter", in.filter_type, out.filter_type);
        symbol_length = m.symbols;
        if (symbol_length <= 0)
            symbol_length = (int32_t)((1u << std::clamp(m.pn_order, 4, 16)) - 1);
        break;
    }
    case mod_settings::OFDM: {
        ofdm_profile in, out;
        mod_init_ofdm(dev, &in);
        in.idleinterval = m.ofdm_idle;
        in.fftsize = m.ofdm_fft;
        in.symbolcount = m.ofdm_symbols;
        in.guardbandcarriers_left = m.ofdm_guard_left;
        in.guardbandcarriers_right = m.ofdm_guard_right;
        in.guardinterval = m.ofdm_cp;
        in.windowed = m.ofdm_window > 0 ? 1 : 0;
        if (m.ofdm_window > 0)
            in.window_length = m.ofdm_window;
        in.digitalmod_type = (uint8_t)m.ofdm_mod;
        in.nulldc = m.ofdm_null_dc ? 1 : 0;
        in.samplerate = m.ofdm_rate;
        out = in;
        cfg(mod_config_ofdm(dev, &in, &out), "mod_config_ofdm");
        adj("idle time", in.idleinterval, out.idleinterval);
        adj("FFT size", in.fftsize, out.fftsize);
        adj("symbols", in.symbolcount, out.symbolcount);
        adj("left guard carriers", in.guardbandcarriers_left, out.guardbandcarriers_left);
        adj("right guard carriers", in.guardbandcarriers_right, out.guardbandcarriers_right);
        adj("guard interval", in.guardinterval, out.guardinterval);
        adj("window length", in.window_length, out.window_length);
        adj("subcarrier modulation", in.digitalmod_type, out.digitalmod_type);
        adj("sample rate", in.samplerate, out.samplerate);
        break;
    }
    default:
        throw std::invalid_argument("harogic_sg: unknown built-in waveform");
    }

    int16_t* iq = nullptr;
    int32_t len = 0;
    double rate = 0.0;
    check(mod_generate_stream(dev, symbol_length, &iq, &len, &rate), "mod_generate_stream");
    if (!iq || len < 2 || rate <= 0)
        throw std::runtime_error(std::string("harogic_sg: ") + mod_settings::name(m.wave) +
                                 ": the modulation library returned no samples");

    const size_t points = (size_t)len / 2;
    if (d_caps_valid && d_caps.memory_size > 0 &&
        points * 2 * sizeof(int16_t) > d_caps.memory_size)
        throw std::runtime_error(
            "harogic_sg: " + std::string(mod_settings::name(m.wave)) + " waveform of " +
            std::to_string(points) + " samples (" +
            std::to_string(points * 4 / 1048576.0) +
            " MB) does not fit in the generator memory (" +
            std::to_string(d_caps.memory_size / 1048576.0) +
            " MB): use fewer symbols or a shorter length");
    if (d_caps_valid && d_caps.playback_srate_max > 0 &&
        (rate < d_caps.playback_srate_min - 1 || rate > d_caps.playback_srate_max + 1))
        d_logger->warn("{}: sample rate {:g} S/s is outside the playback range {:g}..{:g}",
                       mod_settings::name(m.wave),
                       rate,
                       d_caps.playback_srate_min,
                       d_caps.playback_srate_max);

    d_wave.assign(iq, iq + 2 * points);
    d_gen_rate = rate;
    d_gen_key = key;
    d_logger->info("{}: {} samples at {:g} S/s ({:.3f} ms loop)",
                   mod_settings::name(m.wave),
                   points,
                   rate,
                   1e3 * points / rate);
}

void h2_device::arm(const tx_plan& p)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    channel* ch = &d_ch[d_tx];
    const bool playback = (p.kind == tx_plan::PLAYBACK);

    // Generate before stopping: a bad parameter leaves the current output alone.
    if (playback)
        generate(p.mod);

    check(channel_stop(ch), "channel_stop", false);

    switch (p.kind) {
    case tx_plan::FREQ_SWEEP:
        check(tx_config_fscan(ch, p.sweep_start, p.sweep_stop, p.sweep_step,
                              (float)p.level, (float)p.dwell),
              "tx_config_fscan");
        break;
    case tx_plan::LEVEL_SWEEP:
        check(tx_config_lscan(ch, p.frequency, (float)p.level_start,
                              (float)p.level_stop, (float)p.level_step, (float)p.dwell),
              "tx_config_lscan");
        break;
    default:
        check(tx_config_ffm(ch, p.frequency, (float)p.level), "tx_config_ffm");
        break;
    }

    if (playback && d_loaded_key != d_gen_key) {
        // The memory only appends (waveform 0, 1, ...): clear, then load ours as 0.
        d_loaded_key.clear();
        check(tx_clear_waveform(&d_dev), "tx_clear_waveform");
        uint16_t index = 0;
        check(tx_download_waveform(&d_dev, d_wave.data(), (uint32_t)(d_wave.size() / 2),
                                   &index),
              "tx_download_waveform");
        d_pb_waveform = (int16_t)index;
        d_loaded_key = d_gen_key;
        d_wave_rate = d_gen_rate;
        d_wave_points = d_wave.size() / 2;
    }

    std::memset(&d_stream, 0, sizeof(d_stream));
    std::memset(&d_trigger, 0, sizeof(d_trigger));
    d_stream.state = STATE_ON;
    d_stream.mode = (p.kind == tx_plan::REALTIME) ? TX_REALTIME
                    : playback                   ? TX_PLAYBACK
                                                 : TX_CW;
    d_stream.fc_offset = p.freq_offset;
    d_stream.phase_offset = (float)p.phase_offset;
    if (playback) {
        d_pb_repeat = -1; // loop forever
        d_pb_srate = d_wave_rate;
        d_stream.waveforms = 1;
        d_stream.waveform = &d_pb_waveform;
        d_stream.repeat = &d_pb_repeat;
        d_stream.playback_srate = &d_pb_srate;
    } else {
        d_stream.realtime_srate = p.samp_rate;
    }
    d_trigger.source = TRIGGER_SOURCE_BUS;
    d_trigger.response_count = -1;
    check(tx_config_stream(ch, 0, &d_stream, &d_trigger), "tx_config_stream");
    check(channel_config_trigger(ch, 0, TRIGGER_ACTION_SWEEP), "channel_config_trigger");

    const bool mod = (p.kind == tx_plan::REALTIME) || playback;
    check(tx_config_output(ch, p.rf_on ? STATE_ON : STATE_OFF,
                           (p.rf_on && mod) ? STATE_ON : STATE_OFF),
          "tx_config_output");
    check(channel_start(ch), "channel_start");
    check(channel_bus_trigger(ch, 0), "channel_bus_trigger");
}

void h2_device::set_lo_mode(int mode, const tx_plan& p)
{
    {
        std::lock_guard<std::mutex> lock(d_mutex);
        d_settings.lo_mode = mode;
        check(channel_config_lo_mode(&d_ch[d_tx], (uint8_t)mode),
              "channel_config_lo_mode");
    }
    arm(p);
}

void h2_device::emergency_shutdown() noexcept
{
    // Process is exiting: do not log (loggers may already be gone) and do not
    // wait forever on a mutex held by a thread that no longer runs.
    if (!d_dev || d_tx < 0)
        return;
    bool locked = d_mutex.try_lock();
    tx_config_output(&d_ch[d_tx], STATE_OFF, STATE_OFF);
    channel_stop(&d_ch[d_tx]);
    device_close(&d_dev);
    d_dev = nullptr;
    if (locked)
        d_mutex.unlock();
}

void h2_device::rf_off() noexcept
{
    if (!d_dev || d_tx < 0)
        return;
    std::lock_guard<std::mutex> lock(d_mutex);
    tx_config_output(&d_ch[d_tx], STATE_OFF, STATE_OFF);
    channel_stop(&d_ch[d_tx]);
}

int h2_device::send_iq(int16_t* iq, uint32_t points)
{
    // No lock: tx_send_stream runs in the work thread while arm() is serialized
    // by the block itself.
    int rc = tx_send_stream(&d_ch[d_tx], 0, iq, points);
    if (rc < 0)
        throw std::runtime_error("harogic_sg: tx_send_stream failed: " +
                                 h2_error_string(rc));
    return rc;
}

} // namespace harogic_sg
} // namespace gr
