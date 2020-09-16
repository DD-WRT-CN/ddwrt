# Test cases for VHT operations with hostapd
# Copyright (c) 2014, Qualcomm Atheros, Inc.
# Copyright (c) 2013, Intel Corporation
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import os
import subprocess, time

import hwsim_utils
import hostapd
from test_dfs import wait_dfs_event
from test_ap_csa import csa_supported

def vht_supported():
    cmd = subprocess.Popen(["iw", "reg", "get"], stdout=subprocess.PIPE)
    reg = cmd.stdout.read()
    if "@ 80)" in reg or "@ 160)" in reg:
        return True
    return False

def test_ap_vht80(dev, apdev):
    """VHT with 80 MHz channel width"""
    try:
        params = { "ssid": "vht",
                   "country_code": "FI",
                   "hw_mode": "a",
                   "channel": "36",
                   "ht_capab": "[HT40+]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "1",
                   "vht_oper_centr_freq_seg0_idx": "42" }
        hapd = hostapd.add_ap(apdev[0]['ifname'], params)

        dev[0].connect("vht", key_mgmt="NONE", scan_freq="5180")
        hwsim_utils.test_connectivity(dev[0], hapd)
    except Exception, e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                logger.info("80 MHz channel not supported in regulatory information")
                return "skip"
        raise
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])

def test_ap_vht80_params(dev, apdev):
    """VHT with 80 MHz channel width and number of optional features enabled"""
    try:
        params = { "ssid": "vht",
                   "country_code": "FI",
                   "hw_mode": "a",
                   "channel": "36",
                   "ht_capab": "[HT40+][SHORT-GI-40][DSS_CCK-40]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "1",
                   "vht_capab": "[MAX-MPDU-11454][RXLDPC][SHORT-GI-80][TX-STBC-2BY1][RX-STBC-1][MAX-A-MPDU-LEN-EXP0]",
                   "vht_oper_centr_freq_seg0_idx": "42",
                   "require_vht": "1" }
        hapd = hostapd.add_ap(apdev[0]['ifname'], params)

        dev[1].connect("vht", key_mgmt="NONE", scan_freq="5180",
                       disable_vht="1", wait_connect=False)
        dev[0].connect("vht", key_mgmt="NONE", scan_freq="5180")
        ev = dev[1].wait_event(["CTRL-EVENT-ASSOC-REJECT"])
        if ev is None:
            raise Exception("Association rejection timed out")
        if "status_code=104" not in ev:
            raise Exception("Unexpected rejection status code")
        dev[1].request("DISCONNECT")
        hwsim_utils.test_connectivity(dev[0], hapd)
    except Exception, e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                logger.info("80 MHz channel not supported in regulatory information")
                return "skip"
        raise
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])

def test_ap_vht_20(devs, apdevs):
    dev = devs[0]
    ap = apdevs[0]
    try:
        params = { "ssid": "test-vht20",
                   "country_code": "DE",
                   "hw_mode": "a",
                   "channel": "36",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "ht_capab": "",
                   "vht_capab": "",
                   "vht_oper_chwidth": "0",
                   "vht_oper_centr_freq_seg0_idx": "0",
                   "supported_rates": "60 120 240 360 480 540",
                   "require_vht": "1",
                 }
        hapd = hostapd.add_ap(ap['ifname'], params)
        dev.connect("test-vht20", scan_freq="5180", key_mgmt="NONE")
        hwsim_utils.test_connectivity(dev, hapd)
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])

def test_ap_vht_capab_not_supported(dev, apdev):
    """VHT configuration with driver not supporting all vht_capab entries"""
    try:
        params = { "ssid": "vht",
                   "country_code": "FI",
                   "hw_mode": "a",
                   "channel": "36",
                   "ht_capab": "[HT40+][SHORT-GI-40][DSS_CCK-40]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "1",
                   "vht_capab": "[MAX-MPDU-7991][MAX-MPDU-11454][VHT160][VHT160-80PLUS80][RXLDPC][SHORT-GI-80][SHORT-GI-160][TX-STBC-2BY1][RX-STBC-1][RX-STBC-12][RX-STBC-123][RX-STBC-1234][SU-BEAMFORMER][SU-BEAMFORMEE][BF-ANTENNA-2][SOUNDING-DIMENSION-2][MU-BEAMFORMER][MU-BEAMFORMEE][VHT-TXOP-PS][HTC-VHT][MAX-A-MPDU-LEN-EXP0][MAX-A-MPDU-LEN-EXP7][VHT-LINK-ADAPT2][VHT-LINK-ADAPT3][RX-ANTENNA-PATTERN][TX-ANTENNA-PATTERN]",
                   "vht_oper_centr_freq_seg0_idx": "42",
                   "require_vht": "1" }
        hapd = hostapd.add_ap(apdev[0]['ifname'], params, wait_enabled=False)
        ev = hapd.wait_event(["AP-DISABLED"], timeout=5)
        if ev is None:
            raise Exception("Startup failure not reported")
        for i in range(1, 7):
            if "OK" not in hapd.request("SET vht_capab [MAX-A-MPDU-LEN-EXP%d]" % i):
                raise Exception("Unexpected SET failure")
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])

def test_ap_vht160(dev, apdev):
    """VHT with 160 MHz channel width"""
    try:
        params = { "ssid": "vht",
                   "country_code": "FI",
                   "hw_mode": "a",
                   "channel": "36",
                   "ht_capab": "[HT40+]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "2",
                   "vht_oper_centr_freq_seg0_idx": "50",
                   'ieee80211d': '1',
                   'ieee80211h': '1' }
        hapd = hostapd.add_ap(apdev[0]['ifname'], params, wait_enabled=False)

        ev = wait_dfs_event(hapd, "DFS-CAC-START", 5)
        if "DFS-CAC-START" not in ev:
            raise Exception("Unexpected DFS event")

        state = hapd.get_status_field("state")
        if state != "DFS":
            if state == "DISABLED" and not os.path.exists("dfs"):
                # Not all systems have recent enough CRDA version and
                # wireless-regdb changes to support 160 MHz and DFS. For now,
                # do not report failures for this test case.
                return "skip"
            raise Exception("Unexpected interface state: " + state)

        params = { "ssid": "vht2",
                   "country_code": "FI",
                   "hw_mode": "a",
                   "channel": "100",
                   "ht_capab": "[HT40+]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "2",
                   "vht_oper_centr_freq_seg0_idx": "114",
                   'ieee80211d': '1',
                   'ieee80211h': '1' }
        hapd2 = hostapd.add_ap(apdev[1]['ifname'], params, wait_enabled=False)

        ev = wait_dfs_event(hapd2, "DFS-CAC-START", 5)
        if "DFS-CAC-START" not in ev:
            raise Exception("Unexpected DFS event(2)")

        state = hapd2.get_status_field("state")
        if state != "DFS":
            raise Exception("Unexpected interface state(2): " + state)

        logger.info("Waiting for CAC to complete")

        ev = wait_dfs_event(hapd, "DFS-CAC-COMPLETED", 70)
        if "success=1" not in ev:
            raise Exception("CAC failed")
        if "freq=5180" not in ev:
            raise Exception("Unexpected DFS freq result")

        ev = hapd.wait_event(["AP-ENABLED"], timeout=5)
        if not ev:
            raise Exception("AP setup timed out")

        state = hapd.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state")

        ev = wait_dfs_event(hapd2, "DFS-CAC-COMPLETED", 70)
        if "success=1" not in ev:
            raise Exception("CAC failed(2)")
        if "freq=5500" not in ev:
            raise Exception("Unexpected DFS freq result(2)")

        ev = hapd2.wait_event(["AP-ENABLED"], timeout=5)
        if not ev:
            raise Exception("AP setup timed out(2)")

        state = hapd2.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state(2)")

        freq = hapd2.get_status_field("freq")
        if freq != "5500":
            raise Exception("Unexpected frequency(2)")

        dev[0].connect("vht", key_mgmt="NONE", scan_freq="5180")
        hwsim_utils.test_connectivity(dev[0], hapd)
        dev[1].connect("vht2", key_mgmt="NONE", scan_freq="5500")
        hwsim_utils.test_connectivity(dev[1], hapd2)
    except Exception, e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                logger.info("80/160 MHz channel not supported in regulatory information")
                return "skip"
        raise
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])

def test_ap_vht80plus80(dev, apdev):
    """VHT with 80+80 MHz channel width"""
    try:
        params = { "ssid": "vht",
                   "country_code": "US",
                   "hw_mode": "a",
                   "channel": "52",
                   "ht_capab": "[HT40+]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "3",
                   "vht_oper_centr_freq_seg0_idx": "58",
                   "vht_oper_centr_freq_seg1_idx": "155",
                   'ieee80211d': '1',
                   'ieee80211h': '1' }
        hapd = hostapd.add_ap(apdev[0]['ifname'], params, wait_enabled=False)
        # This will actually fail since DFS on 80+80 is not yet supported
        ev = hapd.wait_event(["AP-DISABLED"], timeout=5)
        # ignore result to avoid breaking the test once 80+80 DFS gets enabled

        params = { "ssid": "vht2",
                   "country_code": "US",
                   "hw_mode": "a",
                   "channel": "36",
                   "ht_capab": "[HT40+]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "3",
                   "vht_oper_centr_freq_seg0_idx": "42",
                   "vht_oper_centr_freq_seg1_idx": "155" }
        hapd2 = hostapd.add_ap(apdev[1]['ifname'], params, wait_enabled=False)

        ev = hapd2.wait_event(["AP-ENABLED"], timeout=5)
        if not ev:
            raise Exception("AP setup timed out(2)")

        state = hapd2.get_status_field("state")
        if state != "ENABLED":
            raise Exception("Unexpected interface state(2)")

        dev[1].connect("vht2", key_mgmt="NONE", scan_freq="5180")
        hwsim_utils.test_connectivity(dev[1], hapd2)
    except Exception, e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                logger.info("80/160 MHz channel not supported in regulatory information")
                return "skip"
        raise
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])

def test_ap_vht80_csa(dev, apdev):
    """VHT with 80 MHz channel width and CSA"""
    if not csa_supported(dev[0]):
        return "skip"
    try:
        params = { "ssid": "vht",
                   "country_code": "US",
                   "hw_mode": "a",
                   "channel": "149",
                   "ht_capab": "[HT40+]",
                   "ieee80211n": "1",
                   "ieee80211ac": "1",
                   "vht_oper_chwidth": "1",
                   "vht_oper_centr_freq_seg0_idx": "155" }
        hapd = hostapd.add_ap(apdev[0]['ifname'], params)

        dev[0].connect("vht", key_mgmt="NONE", scan_freq="5745")
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5180 ht vht blocktx center_freq1=5210 sec_channel_offset=1 bandwidth=80")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5180" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        time.sleep(0.5)
        hwsim_utils.test_connectivity(dev[0], hapd)

        hapd.request("CHAN_SWITCH 5 5745")
        ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
        if ev is None:
            raise Exception("CSA finished event timed out")
        if "freq=5745" not in ev:
            raise Exception("Unexpected channel in CSA finished event")
        time.sleep(0.5)
        hwsim_utils.test_connectivity(dev[0], hapd)

        # This CSA to same channel will fail in kernel, so use this only for
        # extra code coverage.
        hapd.request("CHAN_SWITCH 5 5745")
        hapd.wait_event(["AP-CSA-FINISHED"], timeout=1)
    except Exception, e:
        if isinstance(e, Exception) and str(e) == "AP startup failed":
            if not vht_supported():
                logger.info("80 MHz channel not supported in regulatory information")
                return "skip"
        raise
    finally:
        subprocess.call(['sudo', 'iw', 'reg', 'set', '00'])
