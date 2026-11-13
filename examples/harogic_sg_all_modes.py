#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Harogic SG - all modes
# Author: Penthertz
# Description: Switch between a GNU Radio FM signal and the generator CW / frequency sweep / level sweep (gr-harogic_sg)
# GNU Radio version: 3.10.12.0

from PyQt5 import Qt
from gnuradio import qtgui
from PyQt5 import QtCore
from PyQt5.QtCore import QObject, pyqtSlot
from gnuradio import harogic_sg
import math
import osmosdr
import time
import sip
import threading
from gnuradio import gr
from gnuradio.filter import firdes
from gnuradio.fft import window
import sys
import signal
from argparse import ArgumentParser
from gnuradio.eng_arg import eng_float, intx
from gnuradio import eng_notation




class harogic_sg_all_modes(gr.top_block, Qt.QWidget):

    def __init__(self):
        gr.top_block.__init__(self, "Harogic SG - all modes", catch_exceptions=True)
        Qt.QWidget.__init__(self)
        self.setWindowTitle("Harogic SG - all modes")
        qtgui.util.check_set_qss()
        try:
            self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except BaseException as exc:
            print(f"Qt GUI: Could not set Icon: {str(exc)}", file=sys.stderr)
        self.top_scroll_layout = Qt.QVBoxLayout()
        self.setLayout(self.top_scroll_layout)
        self.top_scroll = Qt.QScrollArea()
        self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
        self.top_scroll_layout.addWidget(self.top_scroll)
        self.top_scroll.setWidgetResizable(True)
        self.top_widget = Qt.QWidget()
        self.top_scroll.setWidget(self.top_widget)
        self.top_layout = Qt.QVBoxLayout(self.top_widget)
        self.top_grid_layout = Qt.QGridLayout()
        self.top_layout.addLayout(self.top_grid_layout)

        self.settings = Qt.QSettings("gnuradio/flowgraphs", "harogic_sg_all_modes")

        try:
            geometry = self.settings.value("geometry")
            if geometry:
                self.restoreGeometry(geometry)
        except BaseException as exc:
            print(f"Qt GUI: Could not restore geometry: {str(exc)}", file=sys.stderr)
        self.flowgraph_started = threading.Event()

        ##################################################
        # Variables
        ##################################################
        self.tone = tone = 1000
        self.samp_rate = samp_rate = 1e6
        self.rf_on = rf_on = True
        self.mode = mode = 0
        self.level = level = -30
        self.freq = freq = 868.3e6
        self.dwell = dwell = 0.25
        self.deviation = deviation = 5000

        ##################################################
        # Blocks
        ##################################################

        _rf_on_check_box = Qt.QCheckBox("RF output")
        self._rf_on_choices = {True: True, False: False}
        self._rf_on_choices_inv = dict((v,k) for k,v in self._rf_on_choices.items())
        self._rf_on_callback = lambda i: Qt.QMetaObject.invokeMethod(_rf_on_check_box, "setChecked", Qt.Q_ARG("bool", self._rf_on_choices_inv[i]))
        self._rf_on_callback(self.rf_on)
        _rf_on_check_box.stateChanged.connect(lambda i: self.set_rf_on(self._rf_on_choices[bool(i)]))
        self.top_grid_layout.addWidget(_rf_on_check_box, 1, 1, 1, 1)
        for r in range(1, 2):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(1, 2):
            self.top_grid_layout.setColumnStretch(c, 1)
        # Create the options list
        self._mode_options = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
        # Create the labels list
        self._mode_labels = ['GNU Radio FM', 'CW', 'Frequency sweep', 'Level sweep', 'AM', 'FM', 'PM', 'Pulse', 'Multitone', 'Ramp sweep', 'AWGN', 'Digital mod (QPSK)', 'DSSS', 'OFDM']
        # Create the combo box
        self._mode_tool_bar = Qt.QToolBar(self)
        self._mode_tool_bar.addWidget(Qt.QLabel("Mode" + ": "))
        self._mode_combo_box = Qt.QComboBox()
        self._mode_tool_bar.addWidget(self._mode_combo_box)
        for _label in self._mode_labels: self._mode_combo_box.addItem(_label)
        self._mode_callback = lambda i: Qt.QMetaObject.invokeMethod(self._mode_combo_box, "setCurrentIndex", Qt.Q_ARG("int", self._mode_options.index(i)))
        self._mode_callback(self.mode)
        self._mode_combo_box.currentIndexChanged.connect(
            lambda i: self.set_mode(self._mode_options[i]))
        # Create the radio buttons
        self.top_grid_layout.addWidget(self._mode_tool_bar, 4, 0, 1, 2)
        for r in range(4, 5):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(0, 2):
            self.top_grid_layout.setColumnStretch(c, 1)
        self._level_range = qtgui.Range(-60, 0, 1, -30, 200)
        self._level_win = qtgui.RangeWidget(self._level_range, self.set_level, "Level (dBm)", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_grid_layout.addWidget(self._level_win, 1, 0, 1, 1)
        for r in range(1, 2):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(0, 1):
            self.top_grid_layout.setColumnStretch(c, 1)
        self._freq_range = qtgui.Range(868.0e6, 868.6e6, 10e3, 868.3e6, 200)
        self._freq_win = qtgui.RangeWidget(self._freq_range, self.set_freq, "Frequency (Hz)", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_grid_layout.addWidget(self._freq_win, 0, 0, 1, 2)
        for r in range(0, 1):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(0, 2):
            self.top_grid_layout.setColumnStretch(c, 1)
        self._dwell_range = qtgui.Range(0.01, 2, 0.01, 0.25, 200)
        self._dwell_win = qtgui.RangeWidget(self._dwell_range, self.set_dwell, "Sweep dwell per point (s)", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_grid_layout.addWidget(self._dwell_win, 5, 0, 1, 2)
        for r in range(5, 6):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(0, 2):
            self.top_grid_layout.setColumnStretch(c, 1)
        self._tone_range = qtgui.Range(100, 10000, 100, 1000, 200)
        self._tone_win = qtgui.RangeWidget(self._tone_range, self.set_tone, "Tone (Hz)", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_grid_layout.addWidget(self._tone_win, 2, 0, 1, 1)
        for r in range(2, 3):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(0, 1):
            self.top_grid_layout.setColumnStretch(c, 1)
        self.qtgui_sink_x_0 = qtgui.sink_c(
            1024, #fftsize
            window.WIN_BLACKMAN_hARRIS, #wintype
            0, #fc
            samp_rate, #bw
            "", #name
            True, #plotfreq
            True, #plotwaterfall
            True, #plottime
            True, #plotconst
            None # parent
        )
        self.qtgui_sink_x_0.set_update_time(1.0/10)
        self._qtgui_sink_x_0_win = sip.wrapinstance(self.qtgui_sink_x_0.qwidget(), Qt.QWidget)

        self.qtgui_sink_x_0.enable_rf_freq(False)

        self.top_layout.addWidget(self._qtgui_sink_x_0_win)
        self.osmosdr_source_0 = osmosdr.source(
            args="numchan=" + str(1) + " " + ""
        )
        self.osmosdr_source_0.set_time_unknown_pps(osmosdr.time_spec_t())
        self.osmosdr_source_0.set_sample_rate(samp_rate)
        self.osmosdr_source_0.set_center_freq(freq, 0)
        self.osmosdr_source_0.set_freq_corr(0, 0)
        self.osmosdr_source_0.set_dc_offset_mode(0, 0)
        self.osmosdr_source_0.set_iq_balance_mode(0, 0)
        self.osmosdr_source_0.set_gain_mode(False, 0)
        self.osmosdr_source_0.set_gain(10, 0)
        self.osmosdr_source_0.set_if_gain(20, 0)
        self.osmosdr_source_0.set_bb_gain(20, 0)
        self.osmosdr_source_0.set_antenna('', 0)
        self.osmosdr_source_0.set_bandwidth(0, 0)
        self.harogic_sg_sink_0 = harogic_sg.sink(
            device_index=0,
            mode=13,
            frequency=freq,
            level=level,
            samp_rate=samp_rate,
            rf_on=rf_on,
            scale=0.9,
            sweep_start=868.0e6,
            sweep_stop=868.6e6,
            sweep_step=100e3,
            level_start=(-50),
            level_stop=(-20),
            level_step=2,
            dwell=dwell,
            freq_offset=0,
            phase_offset=0,
            lo_mode=0,
            rf_port=0,
            ext_ref=False,
            ref_freq=10e6,
            ref_out=False,
            max_latency_ms=50,
            mod_rate=1e3,
            mod_shape=1,
            am_depth=50,
            fm_deviation=10e3,
            pm_deviation=1.0,
            pulse_width=(10e-6),
            pulse_period=(100e-6),
            tone_count=8,
            tone_spacing=100e3,
            tone_phase=2,
            ramp_span=1e6,
            ramp_time=(1e-3),
            ramp_period=(1e-3),
            noise_bw=1e6,
            noise_length=(10e-3),
            dig_type=3,
            symbol_rate=100e3,
            fsk_deviation=25e3,
            filter_type=3,
            filter_alpha=0.35,
            sps=4,
            filter_span=8,
            pn_order=9,
            pn_seed=1,
            symbols=0,
            dsss_code=5,
            ofdm_fft=64,
            ofdm_symbols=100,
            ofdm_guard_left=6,
            ofdm_guard_right=5,
            ofdm_cp=25,
            ofdm_window=0,
            ofdm_null_dc=True,
            ofdm_mod=3,
            ofdm_idle=0,
            ofdm_rate=20e6)
        self._deviation_range = qtgui.Range(100, 75000, 100, 5000, 200)
        self._deviation_win = qtgui.RangeWidget(self._deviation_range, self.set_deviation, "Deviation (Hz)", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_grid_layout.addWidget(self._deviation_win, 2, 1, 1, 1)
        for r in range(2, 3):
            self.top_grid_layout.setRowStretch(r, 1)
        for c in range(1, 2):
            self.top_grid_layout.setColumnStretch(c, 1)


        ##################################################
        # Connections
        ##################################################
        self.connect((self.osmosdr_source_0, 0), (self.qtgui_sink_x_0, 0))


    def closeEvent(self, event):
        self.settings = Qt.QSettings("gnuradio/flowgraphs", "harogic_sg_all_modes")
        self.settings.setValue("geometry", self.saveGeometry())
        self.stop()
        self.wait()

        event.accept()

    def get_tone(self):
        return self.tone

    def set_tone(self, tone):
        self.tone = tone

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.harogic_sg_sink_0.set_samp_rate(self.samp_rate)
        self.osmosdr_source_0.set_sample_rate(self.samp_rate)
        self.qtgui_sink_x_0.set_frequency_range(0, self.samp_rate)

    def get_rf_on(self):
        return self.rf_on

    def set_rf_on(self, rf_on):
        self.rf_on = rf_on
        self._rf_on_callback(self.rf_on)
        self.harogic_sg_sink_0.set_rf_on(self.rf_on)

    def get_mode(self):
        return self.mode

    def set_mode(self, mode):
        self.mode = mode
        self._mode_callback(self.mode)

    def get_level(self):
        return self.level

    def set_level(self, level):
        self.level = level
        self.harogic_sg_sink_0.set_level(self.level)

    def get_freq(self):
        return self.freq

    def set_freq(self, freq):
        self.freq = freq
        self.harogic_sg_sink_0.set_frequency(self.freq)
        self.osmosdr_source_0.set_center_freq(self.freq, 0)

    def get_dwell(self):
        return self.dwell

    def set_dwell(self, dwell):
        self.dwell = dwell
        self.harogic_sg_sink_0.set_dwell(self.dwell)

    def get_deviation(self):
        return self.deviation

    def set_deviation(self, deviation):
        self.deviation = deviation




def main(top_block_cls=harogic_sg_all_modes, options=None):

    qapp = Qt.QApplication(sys.argv)

    tb = top_block_cls()

    tb.start()
    tb.flowgraph_started.set()

    tb.show()

    def sig_handler(sig=None, frame=None):
        tb.stop()
        tb.wait()

        Qt.QApplication.quit()

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    timer = Qt.QTimer()
    timer.start(500)
    timer.timeout.connect(lambda: None)

    qapp.exec_()

if __name__ == '__main__':
    main()
