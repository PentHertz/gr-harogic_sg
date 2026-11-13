# gr-harogic_sg

GNU Radio 3.10 out-of-tree module for **Harogic H2 signal generators** (USB
`367f:0101`…`0108`, e.g. model 132 / A60_R1), built on Harogic's H2 API
(`SG_Linux_API`, `libh2api` 2.0.40).

One block, **Harogic SG: Sink** (`harogic_sg.sink`), with a **Mode** selector
(changeable while running, e.g. from a QT GUI Chooser):

| Mode | What is transmitted |
|---|---|
| 0: GNU Radio IQ | the complex input, streamed in real time (195.3125 kS/s … 62.5 MS/s) |
| 1: CW | carrier at Frequency / Level (generator's built-in mode) |
| 2: Frequency Sweep | Sweep Start → Stop in Sweep Step, Dwell per point, repeated (FSCAN) |
| 3: Level Sweep | at Frequency, Level Start → Stop in Level Step, Dwell per point (LSCAN) |

So you can build any signal in GNU Radio and send it, or use the generator's own
CW / sweeps, and switch between them at runtime. Every hardware setting is
exposed in GRC and adjustable while running.

## Requirements

* GNU Radio 3.10, CMake ≥ 3.16, a C++17 compiler, pybind11
* Harogic H2 API: unzip `SG_Linux_API.zip` and run `sudo sh install_h2_lib.sh`
  (installs `/opt/h2api`, `/etc/h2usb_enum.conf` and the udev rule).
  `/etc/h2usb_enum.conf` is mandatory: without it libh2api never enumerates USB.
* Calibration: libh2api loads `<directory of the running program>/CalFile/<model>.ini`.
  For GNU Radio that is `/usr/bin/CalFile/<model>.ini` (python3 lives in /usr/bin).
  Copy the `.ini` files from SGStudio's `bin/CalFile/` there. The blocks log a
  warning if the file is missing (output level accuracy is then degraded).

## Build

```sh
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..      # -DH2API_ROOT=<dir> if not /opt/h2api
make -j$(nproc) && sudo make install && sudo ldconfig
```

## Usage

Close SGStudio first: only one program can open the generator.

### Parameters

| Parameter | Meaning |
|---|---|
| Device Index | USB device number (0 = first generator) |
| Mode | GNU Radio IQ, CW, Frequency Sweep, Level Sweep |
| Frequency (Hz), Level (dBm) | RF frequency and output level |
| Sample Rate (S/s) | IQ rate, 195.3125 kS/s … 62.5 MS/s |
| RF Output | On / Off |
| IQ Scale | 1.0 maps \|x\| = 1.0 to int16 full scale (values are clipped) |
| Sweep Start / Stop / Step (Hz) | frequency sweep |
| Level Start / Stop / Step (dB) | level sweep |
| Dwell per Point (s) | both sweeps |
| Freq. Offset, Phase Offset | digital offsets applied by the generator *(Advanced)* |
| LO Mode | Auto, High Speed, Low Spur, Low Phase Noise *(Advanced)* |
| RF Port | Standard / High Power (only on units with that option) *(Advanced)* |
| Max Latency (ms) | IQ kept queued in the API (default 50 ms) *(Advanced)* |
| Reference Clock, Ext. Ref. Frequency, Reference Output | *(Reference tab)* |

GRC only shows the parameters that matter for the selected mode, and the stream
input is optional (hidden outside GNU Radio IQ mode). In CW / sweep modes a
connected input is consumed at the sample rate and discarded, so a flowgraph can
keep its GNU Radio chain connected and switch modes at runtime.

In GNU Radio IQ mode the block paces itself against the sample rate (the H2 API
queues everything without back-pressure), so **no Throttle block is needed**.
Samples are sent in ~5 ms chunks; measured on a model 132 over USB 3: 1, 10, 25
and 62.5 MS/s sustained.

Message port `cmd` accepts a dict with any of `mode`, `freq`, `level`, `rf`,
`samp_rate`, `freq_offset`, `phase_offset`, `lo_mode`, `sweep_start`,
`sweep_stop`, `sweep_step`, `level_start`, `level_stop`, `level_step`, `dwell`,
e.g. `{'mode': 1, 'freq': 868.3e6, 'level': -20, 'rf': True}`.

The generator is opened when the block is created, so a CW / sweep-only
flowgraph with nothing connected works too.

```python
from gnuradio import gr, analog, harogic_sg
tb = gr.top_block()
src = analog.sig_source_c(1e6, analog.GR_COS_WAVE, 100e3, 0.7)
snk = harogic_sg.sink(mode=0, frequency=868.3e6, level=-30, samp_rate=1e6)
tb.connect(src, snk)
tb.start()            # GNU Radio tone at 868.4 MHz
snk.set_mode(1)       # generator CW at 868.3 MHz
snk.set_sweep(868.0e6, 868.6e6, 100e3); snk.set_mode(2)   # frequency sweep
snk.set_mode(0)       # back to the GNU Radio signal
tb.stop(); tb.wait()
```

### RF off on exit

The generator keeps transmitting when a program exits without switching RF off
(SGStudio does this). The block switches RF off on `stop()`, on destruction and
from an `atexit()` handler, so Ctrl-C, GRC's Kill button (SIGTERM) and a normal
exit all leave the generator silent. Only `SIGKILL` cannot be caught.

## Examples

Installed in `share/gr-harogic_sg/examples/` (QT GUI flowgraphs, 868 MHz SRD band):

* `harogic_sg_fm_tx.grc` — FM tone transmitter with frequency / level / tone /
  deviation sliders and an RF on/off check box
* `harogic_sg_all_modes.grc` — same FM chain plus a Mode chooser to switch at
  runtime between the GNU Radio FM signal, CW, frequency sweep and level sweep

## Notes on the H2 API (learned while writing this module)

* Every TX change needs a full re-arm: `channel_stop` → `tx_config_ffm` /
  `tx_config_fscan` / `tx_config_lscan` → `tx_config_stream` →
  `channel_config_trigger(strm 0, TRIGGER_ACTION_SWEEP)` → `tx_config_output` →
  `channel_start` → `channel_bus_trigger(0)`. Without `channel_config_trigger`
  every call returns success but the output stays silent; calling
  `tx_config_ffm` alone while running mutes the output.
* `tx_send_stream` has no back-pressure (it queued 10 s of IQ in 0.5 s) and is
  much faster with large transfers (8k samples ≈ 39 MS/s, 256k ≈ 78 MS/s).
* `channel_power_state` is declared in `h2_api.h` but not exported by
  `libh2api.so` 2.0.40.

## License

GPL-3.0-or-later
