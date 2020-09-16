# cfg80211 test cases
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import binascii
import os
import subprocess
import time

import hostapd
from nl80211 import *

def nl80211_command(dev, cmd, attr):
    res = dev.request("VENDOR ffffffff {} {}".format(nl80211_cmd[cmd],
                                                     binascii.hexlify(attr)))
    if "FAIL" in res:
        raise Exception("nl80211 command failed")
    return binascii.unhexlify(res)

def test_cfg80211_disassociate(dev, apdev):
    """cfg80211 disassociation command"""
    hapd = hostapd.add_ap(apdev[0]['ifname'], { "ssid": "open" })
    dev[0].connect("open", key_mgmt="NONE", scan_freq="2412")
    ev = hapd.wait_event([ "AP-STA-CONNECTED" ], timeout=5)
    if ev is None:
        raise Exception("No connection event received from hostapd")

    ifindex = int(dev[0].get_driver_status_field("ifindex"))
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr_u16('REASON_CODE', 1)
    attrs += build_nl80211_attr_mac('MAC', apdev[0]['bssid'])
    nl80211_command(dev[0], 'DISASSOCIATE', attrs)

    ev = hapd.wait_event([ "AP-STA-DISCONNECTED" ], timeout=5)
    if ev is None:
        raise Exception("No disconnection event received from hostapd")

def nl80211_frame(dev, ifindex, frame, freq=None, duration=None, offchannel_tx_ok=False):
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    if freq is not None:
        attrs += build_nl80211_attr_u32('WIPHY_FREQ', freq)
    if duration is not None:
        attrs += build_nl80211_attr_u32('DURATION', duration)
    if offchannel_tx_ok:
        attrs += build_nl80211_attr_flag('OFFCHANNEL_TX_OK')
    attrs += build_nl80211_attr('FRAME', frame)
    return parse_nl80211_attrs(nl80211_command(dev, 'FRAME', attrs))

def nl80211_frame_wait_cancel(dev, ifindex, cookie):
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr('COOKIE', cookie)
    return nl80211_command(dev, 'FRAME_WAIT_CANCEL', attrs)

def nl80211_remain_on_channel(dev, ifindex, freq, duration):
    attrs = build_nl80211_attr_u32('IFINDEX', ifindex)
    attrs += build_nl80211_attr_u32('WIPHY_FREQ', freq)
    attrs += build_nl80211_attr_u32('DURATION', duration)
    return nl80211_command(dev, 'REMAIN_ON_CHANNEL', attrs)

def test_cfg80211_tx_frame(dev, apdev, params):
    """cfg80211 offchannel TX frame command"""
    ifindex = int(dev[0].get_driver_status_field("ifindex"))

    frame = binascii.unhexlify("d000000002000000010002000000000002000000010000000409506f9a090001dd5e506f9a0902020025080401001f0502006414060500585804510b0906000200000000000b1000585804510b0102030405060708090a0b0d1d000200000000000108000000000000000000101100084465766963652041110500585804510bdd190050f204104a0001101012000200011049000600372a000120")

    dev[0].request("P2P_GROUP_ADD freq=2412")
    res = nl80211_frame(dev[0], ifindex, frame, freq=2422, duration=500,
                        offchannel_tx_ok=True)
    time.sleep(0.1)

    # note: Uncommenting this seems to remove the incorrect channel issue
    #nl80211_frame_wait_cancel(dev[0], ifindex, res[nl80211_attr['COOKIE']])

    # note: this Action frame ends up getting sent incorrectly on 2422 MHz
    nl80211_frame(dev[0], ifindex, frame, freq=2412)
    time.sleep(1.5)
    # note: also the Deauthenticate frame sent by the GO going down ends up
    # being transmitted incorrectly on 2422 MHz.

    try:
        arg = [ "tshark",
                "-r", os.path.join(params['logdir'], "hwsim0.pcapng"),
                "-R", "wlan.fc.type_subtype == 13",
                "-Tfields", "-e", "radiotap.channel.freq" ]
        cmd = subprocess.Popen(arg, stdout=subprocess.PIPE,
                               stderr=open('/dev/null', 'w'))
    except Exception, e:
        logger.info("Could run run tshark check: " + str(e))
        cmd = None
        pass

    if cmd:
        freq = cmd.stdout.read().splitlines()
        if len(freq) != 2:
            raise Exception("Unexpected number of Action frames (%d)" % len(freq))
        if freq[0] != "2422":
            raise Exception("First Action frame on unexpected channel: %s MHz" % freq[0])
        if freq[1] != "2412":
            raise Exception("Second Action frame on unexpected channel: %s MHz" % freq[1])
