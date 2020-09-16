# Test cases for FILS
# Copyright (c) 2015-2017, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import hashlib
import logging
logger = logging.getLogger()
import os
import socket
import struct
import time

import hostapd
from wpasupplicant import WpaSupplicant
import hwsim_utils
from utils import HwsimSkip, alloc_fail
from test_erp import check_erp_capa, start_erp_as
from test_ap_hs20 import ip_checksum

def check_fils_capa(dev):
    capa = dev.get_capability("fils")
    if capa is None or "FILS" not in capa:
        raise HwsimSkip("FILS not supported")

def check_fils_sk_pfs_capa(dev):
    capa = dev.get_capability("fils")
    if capa is None or "FILS-SK-PFS" not in capa:
        raise HwsimSkip("FILS-SK-PFS not supported")

def test_fils_sk_full_auth(dev, apdev):
    """FILS SK full authentication"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    bss = dev[0].get_bss(bssid)
    logger.debug("BSS: " + str(bss))
    if "[FILS]" not in bss['flags']:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA256-CCMP]" not in bss['flags']:
        raise Exception("[WPA2-FILS-SHA256-CCMP] flag not indicated")

    res = dev[0].request("SCAN_RESULTS")
    logger.debug("SCAN_RESULTS: " + res)
    if "[FILS]" not in res:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA256-CCMP]" not in res:
        raise Exception("[WPA2-FILS-SHA256-CCMP] flag not indicated")

    dev[0].request("ERP_FLUSH")
    dev[0].connect("fils", key_mgmt="FILS-SHA256",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    conf = hapd.get_config()
    if conf['key_mgmt'] != 'FILS-SHA256':
        raise Exception("Unexpected config key_mgmt: " + conf['key_mgmt'])

def test_fils_sk_sha384_full_auth(dev, apdev):
    """FILS SK full authentication (SHA384)"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    bss = dev[0].get_bss(bssid)
    logger.debug("BSS: " + str(bss))
    if "[FILS]" not in bss['flags']:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA384-CCMP]" not in bss['flags']:
        raise Exception("[WPA2-FILS-SHA384-CCMP] flag not indicated")

    res = dev[0].request("SCAN_RESULTS")
    logger.debug("SCAN_RESULTS: " + res)
    if "[FILS]" not in res:
        raise Exception("[FILS] flag not indicated")
    if "[WPA2-FILS-SHA384-CCMP]" not in res:
        raise Exception("[WPA2-FILS-SHA384-CCMP] flag not indicated")

    dev[0].request("ERP_FLUSH")
    dev[0].connect("fils", key_mgmt="FILS-SHA384",
                   eap="PSK", identity="psk.user@example.com",
                   password_hex="0123456789abcdef0123456789abcdef",
                   erp="1", scan_freq="2412")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    conf = hapd.get_config()
    if conf['key_mgmt'] != 'FILS-SHA384':
        raise Exception("Unexpected config key_mgmt: " + conf['key_mgmt'])

def test_fils_sk_pmksa_caching(dev, apdev):
    """FILS SK and PMKSA caching"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    hwsim_utils.test_connectivity(dev[0], hapd)
    pmksa2 = dev[0].get_pmksa(bssid)
    if pmksa2 is None:
        raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

    # Verify EAPOL reauthentication after FILS authentication
    hapd.request("EAPOL_REAUTH " + dev[0].own_addr())
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not start")
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-SUCCESS"], timeout=5)
    if ev is None:
        raise Exception("EAP authentication did not succeed")
    time.sleep(0.1)
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_pmksa_caching_and_cache_id(dev, apdev):
    """FILS SK and PMKSA caching with Cache Identifier"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "abcd"
    params["radius_server_clients"] = "auth_serv/radius_clients.conf"
    params["radius_server_auth_port"] = '18128'
    params["eap_server"] = "1"
    params["eap_user_file"] = "auth_serv/eap_user.conf"
    params["ca_cert"] = "auth_serv/ca.pem"
    params["server_cert"] = "auth_serv/server.pem"
    params["private_key"] = "auth_serv/server.key"
    params["eap_sim_db"] = "unix:/tmp/hlr_auc_gw.sock"
    params["dh_file"] = "auth_serv/dh.conf"
    params["pac_opaque_encr_key"] = "000102030405060708090a0b0c0d0e0f"
    params["eap_fast_a_id"] = "101112131415161718191a1b1c1d1e1f"
    params["eap_fast_a_id_info"] = "test server"
    params["eap_server_erp"] = "1"
    params["erp_domain"] = "example.com"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")
    res = dev[0].request("PMKSA")
    if "FILS Cache Identifier" not in res:
        raise Exception("PMKSA list does not include FILS Cache Identifier")
    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
        raise Exception("No PMKSA cache entry created")
    if "cache_id" not in pmksa:
        raise Exception("No FILS Cache Identifier listed")
    if pmksa["cache_id"] != "abcd":
        raise Exception("The configured FILS Cache Identifier not seen in PMKSA")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "abcd"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(bssid2, freq=2412)

    dev[0].dump_monitor()
    if "OK" not in dev[0].request("ROAM " + bssid2):
        raise Exception("ROAM failed")

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if bssid2 not in ev:
        raise Exception("Failed to connect to the second AP")

    hwsim_utils.test_connectivity(dev[0], hapd2)
    pmksa2 = dev[0].get_pmksa(bssid2)
    if pmksa2:
        raise Exception("Unexpected extra PMKSA cache added")
    pmksa2 = dev[0].get_pmksa(bssid)
    if not pmksa2:
        raise Exception("Original PMKSA cache entry removed")
    if pmksa['pmkid'] != pmksa2['pmkid']:
        raise Exception("Unexpected PMKID change")

def test_fils_sk_pmksa_caching_ctrl_ext(dev, apdev):
    """FILS SK and PMKSA caching with Cache Identifier and external management"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    hapd_as = start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "ffee"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA384",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    res1 = dev[0].request("PMKSA_GET %d" % id)
    logger.info("PMKSA_GET: " + res1)
    if "UNKNOWN COMMAND" in res1:
        raise HwsimSkip("PMKSA_GET not supported in the build")
    if bssid not in res1:
        raise Exception("PMKSA cache entry missing")
    if "ffee" not in res1:
        raise Exception("FILS Cache Identifier not seen in PMKSA cache entry")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd_as.disable()

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("PMKSA_FLUSH")
    dev[0].request("ERP_FLUSH")
    for entry in res1.splitlines():
        if "OK" not in dev[0].request("PMKSA_ADD %d %s" % (id, entry)):
            raise Exception("Failed to add PMKSA entry")

    bssid2 = apdev[1]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA384"
    params['auth_server_port'] = "18128"
    params['erp_send_reauth_start'] = '1'
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['fils_cache_id'] = "ffee"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(bssid2, freq=2412)
    dev[0].set_network(id, "bssid", bssid2)
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_connected()
    if bssid2 not in ev:
        raise Exception("Unexpected BSS selected")

def test_fils_sk_erp(dev, apdev):
    """FILS SK using ERP"""
    run_fils_sk_erp(dev, apdev, "FILS-SHA256")

def test_fils_sk_erp_sha384(dev, apdev):
    """FILS SK using ERP and SHA384"""
    run_fils_sk_erp(dev, apdev, "FILS-SHA384")

def run_fils_sk_erp(dev, apdev, key_mgmt):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = key_mgmt
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt=key_mgmt,
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_erp_followed_by_pmksa_caching(dev, apdev):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Force the second connection to use ERP by deleting the PMKSA entry.
    dev[0].request("PMKSA_FLUSH")

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

    pmksa = dev[0].get_pmksa(bssid)
    if pmksa is None:
	    raise Exception("No PMKSA cache entry created")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # The third connection is expected to use PMKSA caching for FILS
    # authentication.
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

    pmksa2 = dev[0].get_pmksa(bssid)
    if pmksa2 is None:
	    raise Exception("No PMKSA cache entry found")
    if pmksa['pmkid'] != pmksa2['pmkid']:
	    raise Exception("Unexpected PMKID change")

def test_fils_sk_erp_another_ssid(dev, apdev):
    """FILS SK using ERP and roam to another SSID"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")

    params = hostapd.wpa2_eap_params(ssid="fils2")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].dump_monitor()
    id = dev[0].connect("fils2", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412", wait_connect=False)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_multiple_realms(dev, apdev):
    """FILS SK and multiple realms"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    fils_realms = [ 'r1.example.org', 'r2.EXAMPLE.org', 'r3.example.org',
                    'r4.example.org', 'r5.example.org', 'r6.example.org',
                    'r7.example.org', 'r8.example.org',
                    'example.com',
                    'r9.example.org', 'r10.example.org', 'r11.example.org',
                    'r12.example.org', 'r13.example.org', 'r14.example.org',
                    'r15.example.org', 'r16.example.org' ]
    params['fils_realm'] = fils_realms
    params['fils_cache_id'] = "1234"
    params['hessid'] = bssid
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)

    if "OK" not in dev[0].request("ANQP_GET " + bssid + " 275"):
        raise Exception("ANQP_GET command failed")
    ev = dev[0].wait_event(["GAS-QUERY-DONE"], timeout=10)
    if ev is None:
        raise Exception("GAS query timed out")
    bss = dev[0].get_bss(bssid)

    if 'fils_info' not in bss:
        raise Exception("FILS Indication element information missing")
    if bss['fils_info'] != '02b8':
        raise Exception("Unexpected FILS Information: " + bss['fils_info'])

    if 'fils_cache_id' not in bss:
        raise Exception("FILS Cache Identifier missing")
    if bss['fils_cache_id'] != '1234':
        raise Exception("Unexpected FILS Cache Identifier: " + bss['fils_cache_id'])

    if 'fils_realms' not in bss:
        raise Exception("FILS Realm Identifiers missing")
    expected = ''
    count = 0
    for realm in fils_realms:
        hash = hashlib.sha256(realm.lower()).digest()
        expected += binascii.hexlify(hash[0:2])
        count += 1
        if count == 7:
            break
    if bss['fils_realms'] != expected:
        raise Exception("Unexpected FILS Realm Identifiers: " + bss['fils_realms'])

    if 'anqp_fils_realm_info' not in bss:
        raise Exception("FILS Realm Information ANQP-element not seen")
    info = bss['anqp_fils_realm_info'];
    expected = ''
    for realm in fils_realms:
        hash = hashlib.sha256(realm.lower()).digest()
        expected += binascii.hexlify(hash[0:2])
    if info != expected:
        raise Exception("Unexpected FILS Realm Info ANQP-element: " + info)

    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

# DHCP message op codes
BOOTREQUEST=1
BOOTREPLY=2

OPT_PAD=0
OPT_DHCP_MESSAGE_TYPE=53
OPT_RAPID_COMMIT=80
OPT_END=255

DHCPDISCOVER=1
DHCPOFFER=2
DHCPREQUEST=3
DHCPDECLINE=4
DHCPACK=5
DHCPNAK=6
DHCPRELEASE=7
DHCPINFORM=8

def build_dhcp(req, dhcp_msg, chaddr, giaddr="0.0.0.0",
               ip_src="0.0.0.0", ip_dst="255.255.255.255",
               rapid_commit=True, override_op=None, magic_override=None,
               opt_end=True, extra_op=None):
    proto = '\x08\x00' # IPv4
    _ip_src = socket.inet_pton(socket.AF_INET, ip_src)
    _ip_dst = socket.inet_pton(socket.AF_INET, ip_dst)

    _ciaddr = '\x00\x00\x00\x00'
    _yiaddr = '\x00\x00\x00\x00'
    _siaddr = '\x00\x00\x00\x00'
    _giaddr = socket.inet_pton(socket.AF_INET, giaddr)
    _chaddr = binascii.unhexlify(chaddr.replace(':','')) + 10*'\x00'
    htype = 1 # Hardware address type; 1 = Ethernet
    hlen = 6 # Hardware address length
    hops = 0
    xid = 123456
    secs = 0
    flags = 0
    if req:
        op = BOOTREQUEST
        src_port = 68
        dst_port = 67
    else:
        op = BOOTREPLY
        src_port = 67
        dst_port = 68
    if override_op is not None:
        op = override_op
    payload = struct.pack('>BBBBLHH', op, htype, hlen, hops, xid, secs, flags)
    sname = 64*'\x00'
    file = 128*'\x00'
    payload += _ciaddr + _yiaddr + _siaddr + _giaddr + _chaddr + sname + file
    # magic - DHCP
    if magic_override is not None:
        payload += magic_override
    else:
        payload += '\x63\x82\x53\x63'
    # Option: DHCP Message Type
    if dhcp_msg is not None:
        payload += struct.pack('BBB', OPT_DHCP_MESSAGE_TYPE, 1, dhcp_msg)
    if rapid_commit:
        # Option: Rapid Commit
        payload += struct.pack('BB', OPT_RAPID_COMMIT, 0)
    if extra_op:
        payload += extra_op
    # End Option
    if opt_end:
        payload += struct.pack('B', OPT_END)

    udp = struct.pack('>HHHH', src_port, dst_port,
                      8 + len(payload), 0) + payload

    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4 = start + csum + _ip_src + _ip_dst

    return proto + ipv4 + udp

def fils_hlp_config(fils_hlp_wait_time=10000):
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['own_ip_addr'] = '127.0.0.3'
    params['dhcp_server'] = '127.0.0.2'
    params['fils_hlp_wait_time'] = str(fils_hlp_wait_time)
    return params

def test_fils_sk_hlp(dev, apdev):
    """FILS SK HLP (rapid commit server)"""
    run_fils_sk_hlp(dev, apdev, True)

def test_fils_sk_hlp_no_rapid_commit(dev, apdev):
    """FILS SK HLP (no rapid commit server)"""
    run_fils_sk_hlp(dev, apdev, False)

def run_fils_sk_hlp(dev, apdev, rapid_commit_server):
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config()
    params['fils_hlp_wait_time'] = '10000'
    if not rapid_commit_server:
        params['dhcp_rapid_commit_proxy'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    tests = [ "",
              "q",
              "ff:ff:ff:ff:ff:ff",
              "ff:ff:ff:ff:ff:ff q" ]
    for t in tests:
        if "FAIL" not in dev[0].request("FILS_HLP_REQ_ADD " + t):
            raise Exception("Invalid FILS_HLP_REQ_ADD accepted: " + t)
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    tests = [ "ff:ff:ff:ff:ff:ff aabb",
              "ff:ff:ff:ff:ff:ff " + 255*'cc',
              hapd.own_addr() + " ddee010203040506070809",
              "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc) ]
    for t in tests:
        if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + t):
            raise Exception("FILS_HLP_REQ_ADD failed: " + t)
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)

    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    if rapid_commit_server:
        # TODO: Proper rapid commit response
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
    else:
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
        (msg,addr) = sock.recvfrom(1000)
        logger.debug("Received DHCP message from %s" % str(addr))
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK, rapid_commit=False,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
    ev = dev[0].wait_event(["FILS-HLP-RX"], timeout=10)
    if ev is None:
        raise Exception("FILS HLP response not reported")
    vals = ev.split(' ')
    frame = binascii.unhexlify(vals[3].split('=')[1])
    proto, = struct.unpack('>H', frame[0:2])
    if proto != 0x0800:
        raise Exception("Unexpected ethertype in HLP response: %d" % proto)
    frame = frame[2:]
    ip = frame[0:20]
    if ip_checksum(ip) != '\x00\x00':
        raise Exception("IP header checksum mismatch in HLP response")
    frame = frame[20:]
    udp = frame[0:8]
    frame = frame[8:]
    sport, dport, ulen, ucheck = struct.unpack('>HHHH', udp)
    if sport != 67 or dport != 68:
        raise Exception("Unexpected UDP port in HLP response")
    dhcp = frame[0:28]
    frame = frame[28:]
    op,htype,hlen,hops,xid,secs,flags,ciaddr,yiaddr,siaddr,giaddr = struct.unpack('>4BL2H4L', dhcp)
    chaddr = frame[0:16]
    frame = frame[16:]
    sname = frame[0:64]
    frame = frame[64:]
    file = frame[0:128]
    frame = frame[128:]
    options = frame
    if options[0:4] != '\x63\x82\x53\x63':
        raise Exception("No DHCP magic seen in HLP response")
    options = options[4:]
    # TODO: fully parse and validate DHCPACK options
    if struct.pack('BBB', OPT_DHCP_MESSAGE_TYPE, 1, DHCPACK) not in options:
        raise Exception("DHCPACK not in HLP response")

    dev[0].wait_connected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_timeout(dev, apdev):
    """FILS SK HLP (rapid commit server timeout)"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=30)
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc)):
        raise Exception("FILS_HLP_REQ_ADD failed")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)

    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    # Wait for HLP wait timeout to hit
    # FILS: HLP response timeout - continue with association response
    dev[0].wait_connected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_oom(dev, apdev):
    """FILS SK HLP and hostapd OOM"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=500)
    params['dhcp_rapid_commit_proxy'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc)):
        raise Exception("FILS_HLP_REQ_ADD failed")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "fils_process_hlp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "fils_process_hlp_dhcp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "wpabuf_alloc;fils_process_hlp_dhcp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "wpabuf_alloc;fils_dhcp_handler"):
        dev[0].select_network(id, freq=2412)
        (msg,addr) = sock.recvfrom(1000)
        logger.debug("Received DHCP message from %s" % str(addr))
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "wpabuf_resize;fils_dhcp_handler"):
        dev[0].select_network(id, freq=2412)
        (msg,addr) = sock.recvfrom(1000)
        logger.debug("Received DHCP message from %s" % str(addr))
        dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                              chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
        sock.sendto(dhcpdisc[2+20+8:], addr)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
    with alloc_fail(hapd, 1, "wpabuf_resize;fils_dhcp_request"):
        sock.sendto(dhcpoffer[2+20+8:], addr)
        dev[0].wait_connected()
        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_req_parsing(dev, apdev):
    """FILS SK HLP request parsing"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=30)
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")

    tot_len = 20 + 1
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    _ip_src = '\x00\x00\x00\x00'
    _ip_dst = '\x00\x00\x00\x00'
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4_overflow = start + csum + _ip_src + _ip_dst

    tot_len = 20
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 123)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4_unknown_proto = start + csum + _ip_src + _ip_dst

    tot_len = 20
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    ipv4_missing_udp_hdr = start + csum + _ip_src + _ip_dst

    src_port = 68
    dst_port = 67
    udp = struct.pack('>HHHH', src_port, dst_port, 8 + 1, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    udp_overflow = start + csum + _ip_src + _ip_dst + udp

    udp = struct.pack('>HHHH', src_port, dst_port, 7, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    udp_underflow = start + csum + _ip_src + _ip_dst + udp

    src_port = 123
    dst_port = 456
    udp = struct.pack('>HHHH', src_port, dst_port, 8, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    udp_unknown_port = start + csum + _ip_src + _ip_dst + udp

    src_port = 68
    dst_port = 67
    udp = struct.pack('>HHHH', src_port, dst_port, 8, 0)
    tot_len = 20 + len(udp)
    start = struct.pack('>BBHHBBBB', 0x45, 0, tot_len, 0, 0, 0, 128, 17)
    ipv4 = start + '\x00\x00' + _ip_src + _ip_dst
    csum = ip_checksum(ipv4)
    dhcp_missing_data = start + csum + _ip_src + _ip_dst + udp

    dhcp_not_req = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                              chaddr=dev[0].own_addr(), override_op=BOOTREPLY)
    dhcp_no_magic = build_dhcp(req=True, dhcp_msg=None,
                               chaddr=dev[0].own_addr(), magic_override='',
                               rapid_commit=False, opt_end=False)
    dhcp_unknown_magic = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                                    chaddr=dev[0].own_addr(),
                                    magic_override='\x00\x00\x00\x00')
    dhcp_opts = build_dhcp(req=True, dhcp_msg=DHCPNAK,
                           chaddr=dev[0].own_addr(),
                           extra_op='\x00\x11', opt_end=False)
    dhcp_opts2 = build_dhcp(req=True, dhcp_msg=DHCPNAK,
                            chaddr=dev[0].own_addr(),
                            extra_op='\x11\x01', opt_end=False)
    dhcp_valid = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                            chaddr=dev[0].own_addr())

    tests = [ "ff",
              "0800",
              "0800" + 20*"00",
              "0800" + binascii.hexlify(ipv4_overflow),
              "0800" + binascii.hexlify(ipv4_unknown_proto),
              "0800" + binascii.hexlify(ipv4_missing_udp_hdr),
              "0800" + binascii.hexlify(udp_overflow),
              "0800" + binascii.hexlify(udp_underflow),
              "0800" + binascii.hexlify(udp_unknown_port),
              "0800" + binascii.hexlify(dhcp_missing_data),
              binascii.hexlify(dhcp_not_req),
              binascii.hexlify(dhcp_no_magic),
              binascii.hexlify(dhcp_unknown_magic) ]
    for t in tests:
        if "OK" not in dev[0].request("FILS_HLP_REQ_ADD ff:ff:ff:ff:ff:ff " + t):
            raise Exception("FILS_HLP_REQ_ADD failed: " + t)
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")
    tests = [ binascii.hexlify(dhcp_opts),
              binascii.hexlify(dhcp_opts2) ]
    for t in tests:
        if "OK" not in dev[0].request("FILS_HLP_REQ_ADD ff:ff:ff:ff:ff:ff " + t):
            raise Exception("FILS_HLP_REQ_ADD failed: " + t)

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcp_valid)):
        raise Exception("FILS_HLP_REQ_ADD failed")
    hapd.set("own_ip_addr", "0.0.0.0")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.set("dhcp_server", "0.0.0.0")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: Failed to bind DHCP socket: Address already in use
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))
    hapd.set("own_ip_addr", "127.0.0.2")
    hapd.set("dhcp_server", "127.0.0.2")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: DHCP sendto failed: Invalid argument
    hapd.set("own_ip_addr", "127.0.0.3")
    hapd.set("dhcp_server", "127.0.0.2")
    hapd.set("dhcp_relay_port", "0")
    hapd.set("dhcp_server_port", "0")
    dev[0].select_network(id, freq=2412)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_hlp_dhcp_parsing(dev, apdev):
    """FILS SK HLP and DHCP response parsing"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(5)
    sock.bind(("127.0.0.2", 67))

    bssid = apdev[0]['bssid']
    params = fils_hlp_config(fils_hlp_wait_time=30)
    params['dhcp_rapid_commit_proxy'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    if "OK" not in dev[0].request("FILS_HLP_REQ_FLUSH"):
        raise Exception("Failed to flush pending FILS HLP requests")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr())
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc)):
        raise Exception("FILS_HLP_REQ_ADD failed")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    with alloc_fail(hapd, 1, "fils_process_hlp"):
        dev[0].select_network(id, freq=2412)
        dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpdisc = build_dhcp(req=False, dhcp_msg=DHCPACK,
                          chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
    #sock.sendto(dhcpdisc[2+20+8:], addr)
    chaddr = binascii.unhexlify(dev[0].own_addr().replace(':','')) + 10*'\x00'
    tests = [ "\x00",
              "\x02" + 500 * "\x00",
              "\x02\x00\x00\x00" + 20*"\x00" + "\x7f\x00\x00\x03" + 500 * "\x00",
              "\x02\x00\x00\x00" + 20*"\x00" + "\x7f\x00\x00\x03" + 16*"\x00" + 64*"\x00" + 128*"\x00" + "\x63\x82\x53\x63",
              "\x02\x00\x00\x00" + 20*"\x00" + "\x7f\x00\x00\x03" + 16*"\x00" + 64*"\x00" + 128*"\x00" + "\x63\x82\x53\x63" + "\x00\x11",
              "\x02\x00\x00\x00" + 20*"\x00" + "\x7f\x00\x00\x03" + 16*"\x00" + 64*"\x00" + 128*"\x00" + "\x63\x82\x53\x63" + "\x11\x01",
              "\x02\x00\x00\x00" + 20*"\x00" + "\x7f\x00\x00\x03" + chaddr + 64*"\x00" + 128*"\x00" + "\x63\x82\x53\x63" + "\x35\x00\xff",
              "\x02\x00\x00\x00" + 20*"\x00" + "\x7f\x00\x00\x03" + chaddr + 64*"\x00" + 128*"\x00" + "\x63\x82\x53\x63" + "\x35\x01\x00\xff",
              1501 * "\x00" ]
    for t in tests:
        sock.sendto(t, addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: DHCP sendto failed: Invalid argument for second DHCP TX in proxy
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    hapd.set("dhcp_server_port", "0")
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.set("dhcp_server_port", "67")

    # Options in DHCPOFFER
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op="\x00\x11", opt_end=False)
    sock.sendto(dhcpoffer[2+20+8:], addr)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Options in DHCPOFFER (2)
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op="\x11\x01", opt_end=False)
    sock.sendto(dhcpoffer[2+20+8:], addr)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Server ID in DHCPOFFER
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op="\x36\x01\x30")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: Could not update DHCPDISCOVER
    dev[0].request("FILS_HLP_REQ_FLUSH")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr(),
                          extra_op="\x00\x11", opt_end=False)
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc)):
        raise Exception("FILS_HLP_REQ_ADD failed")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op="\x36\x01\x30")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # FILS: Could not update DHCPDISCOVER (2)
    dev[0].request("FILS_HLP_REQ_FLUSH")
    dhcpdisc = build_dhcp(req=True, dhcp_msg=DHCPDISCOVER,
                          chaddr=dev[0].own_addr(),
                          extra_op="\x11\x01", opt_end=False)
    if "OK" not in dev[0].request("FILS_HLP_REQ_ADD " + "ff:ff:ff:ff:ff:ff " + binascii.hexlify(dhcpdisc)):
        raise Exception("FILS_HLP_REQ_ADD failed")
    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    (msg,addr) = sock.recvfrom(1000)
    logger.debug("Received DHCP message from %s" % str(addr))
    dhcpoffer = build_dhcp(req=False, dhcp_msg=DHCPOFFER, rapid_commit=False,
                           chaddr=dev[0].own_addr(), giaddr="127.0.0.3",
                           extra_op="\x36\x01\x30")
    sock.sendto(dhcpoffer[2+20+8:], addr)
    dev[0].wait_connected()
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].request("FILS_HLP_REQ_FLUSH")

def test_fils_sk_erp_and_reauth(dev, apdev):
    """FILS SK using ERP and AP going away"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['broadcast_deauth'] = '0'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    hapd.disable()
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    hapd.enable()

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Reconnection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")

def test_fils_sk_erp_sim(dev, apdev):
    """FILS SK using ERP with SIM"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    realm='wlan.mnc001.mcc232.3gppnetwork.org'
    start_erp_as(apdev[1], erp_domain=realm)

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['fils_realm'] = realm
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="SIM", identity="1232010000000000@" + realm,
                        password="90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
                        erp="1", scan_freq="2412")

    hapd.disable()
    dev[0].wait_disconnected()
    dev[0].dump_monitor()
    hapd.enable()

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Reconnection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")

def test_fils_sk_pfs_19(dev, apdev):
    """FILS SK with PFS (DH group 19)"""
    rul_fils_sk_pfs(dev, apdev, "19")

def test_fils_sk_pfs_20(dev, apdev):
    """FILS SK with PFS (DH group 20)"""
    rul_fils_sk_pfs(dev, apdev, "20")

def test_fils_sk_pfs_21(dev, apdev):
    """FILS SK with PFS (DH group 21)"""
    rul_fils_sk_pfs(dev, apdev, "21")

def test_fils_sk_pfs_25(dev, apdev):
    """FILS SK with PFS (DH group 25)"""
    rul_fils_sk_pfs(dev, apdev, "25")

def test_fils_sk_pfs_26(dev, apdev):
    """FILS SK with PFS (DH group 26)"""
    rul_fils_sk_pfs(dev, apdev, "26")

def test_fils_sk_pfs_27(dev, apdev):
    """FILS SK with PFS (DH group 27)"""
    rul_fils_sk_pfs(dev, apdev, "27")

def test_fils_sk_pfs_28(dev, apdev):
    """FILS SK with PFS (DH group 28)"""
    rul_fils_sk_pfs(dev, apdev, "28")

def test_fils_sk_pfs_29(dev, apdev):
    """FILS SK with PFS (DH group 29)"""
    rul_fils_sk_pfs(dev, apdev, "29")

def test_fils_sk_pfs_30(dev, apdev):
    """FILS SK with PFS (DH group 30)"""
    rul_fils_sk_pfs(dev, apdev, "30")

def rul_fils_sk_pfs(dev, apdev, group):
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    tls = dev[0].request("GET tls_library")
    if int(group) in [ 27, 28, 29, 30 ]:
        if not (tls.startswith("OpenSSL") and ("build=OpenSSL 1.0.2" in tls or "build=OpenSSL 1.1" in tls) and ("run=OpenSSL 1.0.2" in tls or "run=OpenSSL 1.1" in tls)):
            raise HwsimSkip("Brainpool EC group not supported")

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['fils_dh_group'] = group
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group=group, scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_sk_pfs_group_mismatch(dev, apdev):
    """FILS SK PFS DH group mismatch"""
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params['fils_dh_group'] = "20"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group="19", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-AUTH-REJECT"], timeout=10)
    dev[0].request("DISCONNECT")
    if ev is None:
        raise Exception("Authentication rejection not seen")
    if "auth_type=5 auth_transaction=2 status_code=77" not in ev:
        raise Exception("Unexpected auth reject value: " + ev)

def test_fils_sk_auth_mismatch(dev, apdev):
    """FILS SK authentication type mismatch (PFS not supported)"""
    check_fils_sk_pfs_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", fils_dh_group="19", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" not in ev:
        raise Exception("No EAP exchange seen")
    dev[0].wait_connected()
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_auth_gtk_rekey(dev, apdev):
    """GTK rekeying after FILS authentication"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['wpa_group_rekey'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    dev[0].dump_monitor()
    dev[0].select_network(id, freq=2412)
    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using PMKSA caching timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    dev[0].dump_monitor()

    hwsim_utils.test_connectivity(dev[0], hapd)
    ev = dev[0].wait_event(["WPA: Group rekeying completed"], timeout=2)
    if ev is None:
        raise Exception("GTK rekey timed out")
    hwsim_utils.test_connectivity(dev[0], hapd)

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=5)
    if ev is not None:
        raise Exception("Rekeying failed - disconnected")
    hwsim_utils.test_connectivity(dev[0], hapd)

def test_fils_and_ft(dev, apdev):
    """FILS SK using ERP and FT initial mobility domain association"""
    check_fils_capa(dev[0])
    check_erp_capa(dev[0])

    er = start_erp_as(apdev[1])

    bssid = apdev[0]['bssid']
    params = hostapd.wpa2_eap_params(ssid="fils")
    params['wpa_key_mgmt'] = "FILS-SHA256"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].request("ERP_FLUSH")
    id = dev[0].connect("fils", key_mgmt="FILS-SHA256",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.disable()
    dev[0].flush_scan_cache()
    if "FAIL" in dev[0].request("PMKSA_FLUSH"):
        raise Exception("PMKSA_FLUSH failed")

    params = hostapd.wpa2_eap_params(ssid="fils-ft")
    params['wpa_key_mgmt'] = "FILS-SHA256 FT-FILS-SHA256 FT-EAP"
    params['auth_server_port'] = "18128"
    params['erp_domain'] = 'example.com'
    params['fils_realm'] = 'example.com'
    params['disable_pmksa_caching'] = '1'
    params["mobility_domain"] = "a1b2"
    params["r0_key_lifetime"] = "10000"
    params["pmk_r1_push"] = "1"
    params["reassociation_deadline"] = "1000"
    params['nas_identifier'] = "nas1.w1.fi"
    params['r1_key_holder'] = "000102030405"
    params['r0kh'] = [ "02:00:00:00:04:00 nas2.w1.fi 300102030405060708090a0b0c0d0e0f" ]
    params['r1kh'] = "02:00:00:00:04:00 00:01:02:03:04:06 200102030405060708090a0b0c0d0e0f"
    params['ieee80211w'] = "1"
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    dev[0].scan_for_bss(bssid, freq=2412)
    dev[0].dump_monitor()
    id = dev[0].connect("fils-ft", key_mgmt="FILS-SHA256 FT-FILS-SHA256 FT-EAP",
                        ieee80211w="1",
                        eap="PSK", identity="psk.user@example.com",
                        password_hex="0123456789abcdef0123456789abcdef",
                        erp="1", scan_freq="2412", wait_connect=False)

    ev = dev[0].wait_event(["CTRL-EVENT-EAP-STARTED",
                            "CTRL-EVENT-AUTH-REJECT",
                            "EVENT-ASSOC-REJECT",
                            "CTRL-EVENT-CONNECTED"], timeout=10)
    if ev is None:
        raise Exception("Connection using FILS/ERP timed out")
    if "CTRL-EVENT-EAP-STARTED" in ev:
        raise Exception("Unexpected EAP exchange")
    if "CTRL-EVENT-AUTH-REJECT" in ev:
        raise Exception("Authentication failed")
    if "EVENT-ASSOC-REJECT" in ev:
        raise Exception("Association failed")
    hwsim_utils.test_connectivity(dev[0], hapd)

    er.disable()

    # FIX: FT-FILS-SHA256 does not currently work for FT protocol due to not
    # fully defined FT Reassociation Request/Response frame MIC use in FTE.
    # FT-EAP can be used to work around that in this test case to confirm the
    # FT key hierarchy was properly formed in the previous step.
    #params['wpa_key_mgmt'] = "FILS-SHA256 FT-FILS-SHA256"
    params['wpa_key_mgmt'] = "FT-EAP"
    params['nas_identifier'] = "nas2.w1.fi"
    params['r1_key_holder'] = "000102030406"
    params['r0kh'] = [ "02:00:00:00:03:00 nas1.w1.fi 200102030405060708090a0b0c0d0e0f" ]
    params['r1kh'] = "02:00:00:00:03:00 00:01:02:03:04:05 300102030405060708090a0b0c0d0e0f"
    hapd2 = hostapd.add_ap(apdev[1]['ifname'], params)

    dev[0].scan_for_bss(apdev[1]['bssid'], freq="2412", force_scan=True)
    # FIX: Cannot use FT-over-DS without the FTE MIC issue addressed
    #dev[0].roam_over_ds(apdev[1]['bssid'])
    dev[0].roam(apdev[1]['bssid'])
