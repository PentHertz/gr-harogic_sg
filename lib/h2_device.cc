/* -*- c++ -*- */
/*
 * Copyright 2026 Penthertz.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "h2_device.h"
#include <limits.h>
#include <unistd.h>
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
}

h2_device::~h2_device()
{
    unregister_device(this);
    rf_off();
    if (d_dev) {
        std::lock_guard<std::mutex> lock(d_mutex);
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

void h2_device::arm(const tx_plan& p)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    channel* ch = &d_ch[d_tx];

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

    std::memset(&d_stream, 0, sizeof(d_stream));
    std::memset(&d_trigger, 0, sizeof(d_trigger));
    d_stream.state = STATE_ON;
    d_stream.mode = (p.kind == tx_plan::REALTIME) ? TX_REALTIME : TX_CW;
    d_stream.fc_offset = p.freq_offset;
    d_stream.phase_offset = (float)p.phase_offset;
    d_stream.realtime_srate = p.samp_rate;
    d_trigger.source = TRIGGER_SOURCE_BUS;
    d_trigger.response_count = -1;
    check(tx_config_stream(ch, 0, &d_stream, &d_trigger), "tx_config_stream");
    check(channel_config_trigger(ch, 0, TRIGGER_ACTION_SWEEP), "channel_config_trigger");

    const bool mod = (p.kind == tx_plan::REALTIME);
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
