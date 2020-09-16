/*  Don't edit this file! Source file src/xt_ndpi.h.src */

/* 
 * xt_ndpi.h
 * Copyright (C) 2010-2012 G. Elian Gidoni <geg@gnu.org>
 *               2012 Ed Wildgoose <lists@wildgooses.com>
 * 
 * This file is part of nDPI, an open source deep packet inspection
 * library based on the PACE technology by ipoque GmbH
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _LINUX_NETFILTER_XT_NDPI_H
#define _LINUX_NETFILTER_XT_NDPI_H 1

#include <linux/netfilter.h>
#include "ndpi_main.h"

#ifndef NDPI_BITMASK_IS_ZERO
#define NDPI_BITMASK_IS_ZERO(a) NDPI_BITMASK_IS_EMPTY(a)
#endif

struct xt_ndpi_mtinfo {
	NDPI_PROTOCOL_BITMASK flags;
	unsigned short int invert;
};

struct xt_ndpi_tginfo {
	__u32 mark, mask;
	__u16 proto_id:1, t_accept:1, t_mark:1, t_clsf:1;
};

#ifndef NDPI_PROTOCOL_SHORT_STRING
#define NDPI_PROTOCOL_SHORT_STRING "unknown",	"ftp_control",     "pop3",             "smtp",        "imap",     "dns",\
"ipp",             "http",                 "mdns",             "ntp",           "netbios",\
"nfs",             "ssdp",                 "bgp",              "snmp",          "xdmcp",\
"smbv1",             "syslog",               "dhcp",             "postgres",      "mysql",\
"tds",             "direct_download_link", "pop3_ssl",        "applejuice",    "directconnect",\
"socrates",        "winmx",                "vmware",           "mail_smtps",    "filetopia",\
"imesh",           "kontiki",              "openft",           "fasttrack",     "gnutella",\
"edonkey",         "bittorrent",           "epp",              "avi",           "flash",\
"ogg",             "mpeg",                 "quicktime",        "realmedia",     "windowsmedia",\
"mms",             "xbox",                 "qq",               "move",          "rtsp",\
"mail_imaps",      "icecast",              "pplive",           "ppstream",      "zattoo",\
"shoutcast",       "sopcast",              "tvants",           "tvuplayer",     "http_app_veohtv",\
"qqlive",          "thunder",              "soulseek",         "ssl_no_cert",   "irc",\
"ayiya",           "unencryped_jabber",    "msn",              "oscar",         "yahoo",\
"battlefield",     "quake",                "ip_vrrp",          "steam",         "halflife2",\
"worldofwarcraft", "telnet",               "stun",             "ip_ipsec",      "ip_gre",\
"ip_icmp",         "ip_igmp",              "ip_egp",           "ip_sctp",       "ip_ospf",\
"ip_ip_in_ip",     "rtp",                  "rdp",              "vnc",           "pcanywhere",\
"ssl",             "ssh",                  "usenet",           "mgcp",          "iax",\
"tftp",            "afp",                  "stealthnet",       "aimini",        "sip",\
"truphone",        "ip_icmpv6",            "dhcpv6",           "armagetron",    "crossfire",\
"dofus",           "fiesta",               "florensia",        "guildwars",     "http_app_activesync",\
"kerberos",        "ldap",                 "maplestory",       "mssql",         "pptp",\
"warcraft3",       "world_of_kung_fu",     "meebo",            "facebook",      "twitter",\
"dropbox",         "gmail",                "google_maps",      "youtube",       "skype",\
"google",          "dcerpc",               "netflow",          "sflow",         "http_connect",\
"http_proxy",      "citrix",               "netflix",          "lastfm",        "grooveshark",\
"youtube_upload", "icq",       "checkmk", "citrix_online", "apple",\
"webex",           "whatsapp",             "apple_icloud",     "viber",         "apple_itunes",\
"radius",          "windows_update",       "teamviewer",       "tuenti",        "lotus_notes",\
"sap",             "gtp",                  "upnp",             "llmnr",         "remote_scan",\
"spotify",         "webm",                 "h323",             "openvpn",       "noe",\
"ciscovpn",        "teamspeak",            "tor",              "skinny",        "rtcp",\
"rsync",           "oracle",               "corba",            "ubuntuone",     "whois_das",\
"collectd",        "socks",               "ms_lync",           "rtmp",          "ftp_data",\
"wikipedia",       "zmq",                  "amazon",           "ebay",          "cnn",\
"megaco",          "redis",                "pando",            "vhua",          "telegram", \
"vevo",            "pandora",              "quic",    "popo",           "manolito", "feidian", \
"gadugadu", "i23v5", "secondlife", "whatsapp_voice", "eaq", "timmeu", "torcedor", "kakaotalk", \
"kakaotalk_voice" , "twitch", "quickplay", "tim", "mpegts", "snapchat", "simet","opensignal", \
"99taxi","easytaxi","globotv","timsomdechamada","timmenu","timportasabertas","timrecarga",\
"timbeta","deezer","instagram","microsoft","battlenet","starcraft", "teredo", "hotspot-shield", "hep", "ubntac2", "ocs", "office_365", "cloudflare", "ms_one_drive", "mqtt", "rx", "http_download", \
"coap", "applestore", "opendns", "git", "drda", "playstore", "someip", "fix", "playstation", "pastebin", "linkedin", "soundcloud", "csgo", "lisp", \
"nintendo", "dnscrypt", "1kxun", "iqiyi", "wechat", "github", "sina", "slack", "iflix", "hotmail", "google_drive", "ookla", \
"hangout", "bjnp", "smpp", "tinc", "amqp", "google_plus", "diameter", "apple_push", "google_services", "amazon_video", "google_docs", "whatsapp_files", \
"ajp", "ntop", "facebook_messenger", "musically", "facebook_zero", "vidto", "rapidvideo", "showmax", "smbv23", "mining", "memcached", "signal", "nest_log_sink", "modbus", "line", "tls", "tls_no_cert", "targus_getdata"

#define NDPI_PROTOCOL_MAXNUM 287
#endif

#endif				/* _LINUX_NETFILTER_XT_NDPI_H */
