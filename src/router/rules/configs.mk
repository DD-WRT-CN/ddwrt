obj-$(CONFIG_OMCPROXY) += omcproxy
obj-$(CONFIG_NVRAM) += nvram
obj-$(CONFIG_WIRELESS_TOOLS) += wireless-tools
obj-$(CONFIG_SHARED) += libutils
obj-$(CONFIG_SHARED) += shared
obj-$(CONFIG_LIBNET) += libnet
obj-$(CONFIG_IRQBALANCE) += irqbalance
obj-$(CONFIG_OPENSSL) += openssl openssl-shared openssl-apps
obj-$(CONFIG_MATRIXSSL) += matrixssl
obj-$(CONFIG_CTORRENT) += ctorrent
obj-$(CONFIG_SFTPSERVER) += sftp-server
obj-$(CONFIG_QTN) += qtn
obj-$(CONFIG_IOZONE) += iozone
obj-$(CONFIG_SWCONFIG) += libnltiny swconfig
obj-$(CONFIG_HTTPD) += httpd
obj-$(CONFIG_ATH9K) += libnltiny fft_eval
obj-$(CONFIG_RC) += rc services 
obj-$(CONFIG_LIBBCM) += libbcm
obj-$(CONFIG_WWW) += www
obj-$(CONFIG_GLIBC) += lib.$(ARCH)$(ARCHEXT)
obj-$(CONFIG_UCLIBC) += lib.$(ARCH)$(ARCHEXT)
obj-$(CONFIG_BRIDGE) += bridge
obj-$(CONFIG_RFLOW) += libpcap rflow
obj-$(CONFIG_DROPBEAR_SSHD) += dropbear
obj-$(CONFIG_DHCPFORWARD) += dhcpforwarder
obj-$(CONFIG_BUSYBOX) += busybox
#obj-$(CONFIG_TELNET) += telnetd
obj-$(CONFIG_DNSSEC)  += zlib pcre gmp nettle
obj-$(CONFIG_DNSMASQ) += dnsmasq
obj-$(CONFIG_DNSCRYPT) += libsodium dnscrypt
obj-$(CONFIG_ETHTOOL) += ethtool
obj-$(CONFIG_MOXA) += moxa
ifeq ($(CONFIG_IPV6),y)
obj-$(CONFIG_IPTABLES) += iptables-new
else
obj-$(CONFIG_IPTABLES) += iptables
endif
obj-$(CONFIG_LIBIPT) += iptables-ipt
obj-$(CONFIG_IPSEC) += ipsec
#obj-$(CONFIG_LIBPCAP) += libpcap
obj-$(CONFIG_WIVIZ) += wiviz2
obj-$(CONFIG_TCPDUMP) += libpcap tcpdump
obj-$(CONFIG_KISMETDRONE) += kismet-devel
obj-$(CONFIG_NETSTATNAT) += netstatnat
obj-$(CONFIG_SES) += ses
obj-$(CONFIG_WPA_SUPPLICANT) += wpa_supplicant
obj-$(CONFIG_HOSTAPD) += hostapd
obj-$(CONFIG_NETCONF) += netconf
obj-$(CONFIG_NTP) += ntpclient
obj-$(CONFIG_HTPDATE) += htpdate
obj-$(CONFIG_IPVS) += libnltiny ipvsadm
obj-$(CONFIG_PPP) += ppp
# AhMan March 19 2005
obj-$(CONFIG_PPPOE) += pppoe
#obj-$(CONFIG_UDHCPD) += udhcpd
obj-$(CONFIG_UDHCPC) += udhcpd
obj-$(CONFIG_UPNP) += upnp
ifneq ($(ARCHITECTURE),broadcom)
ifneq ($(CONFIG_ATH5K_AHB),y)
obj-$(CONFIG_MADWIFI) += madwifi relayd
endif
else
obj-$(CONFIG_MADWIFI) += madwifi
endif
obj-$(CONFIG_ETC) += etc
#obj-$(CONFIG_VLAN) += vlan
obj-$(CONFIG_IPROUTE2) += iproute2
obj-$(CONFIG_EBTABLES) += ebtables
obj-$(CONFIG_SSTP) += libevent sstp-client
obj-$(CONFIG_PPTPD) += pptpd
obj-$(CONFIG_PIPSEC) += pipsec
obj-$(CONFIG_FROTTLE) += frottle
obj-$(CONFIG_WOL) += wol
ifeq ($(CONFIG_FREERADIUS),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_ASTERISK),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_AIRCRACK),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_POUND),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_IPETH),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_VPNC),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_TOR),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_UBNTM),y)
obj-$(CONFIG_SNMP) += snmp
endif
ifeq ($(CONFIG_IPV6),y)
obj-$(CONFIG_RADVD) += radvd aiccu
endif
obj-$(CONFIG_L2TPV3TUN) += l2tpv3tun
obj-$(CONFIG_SPUTNIK_APD) += sputnik

#obj-$(CONFIG_ADM6996) += adm6996
##################################################################
CONFIG_OTHERS=y
#obj-$(CONFIG_ADM6996) += adm6996
obj-$(CONFIG_L2TP) += xl2tpd

obj-$(CONFIG_CHILLISPOT) += chillispot
obj-$(CONFIG_PARPROUTED) += parprouted
obj-$(CONFIG_HEARTBEAT) += bpalogin
obj-$(CONFIG_TFTPD) += tftpd
obj-$(CONFIG_CRON) += cron
obj-$(CONFIG_PPTP) += pptp-client
obj-$(CONFIG_PPPD) += pppd
obj-$(CONFIG_ZEBRA) += zebra
obj-$(CONFIG_BIRD) += bird
obj-$(CONFIG_DDNS) += inadyn
obj-$(CONFIG_OTHERS) += others
obj-$(CONFIG_EOU) += eou
obj-$(CONFIG_OPENSER) += openser
obj-$(CONFIG_MILKFISH) += milkfish
obj-$(CONFIG_MC) += libffi zlib glib20 unrar ncurses mc util-linux
obj-$(CONFIG_NOCAT) += nocat
obj-$(CONFIG_POWERTOP) += pciutils ncurses powertop
obj-$(CONFIG_RTPPROXY) += rtpproxy
obj-$(CONFIG_ZABBIX) += pcre zabbix
obj-$(CONFIG_SAMBA) += samba
ifneq ($(CONFIG_SAMBA4),y)
ifneq ($(CONFIG_SMBD),y)
obj-$(CONFIG_SAMBA3) += samba3
endif
endif
obj-$(CONFIG_SMBD) += libnl smbd jansson wsdd2
ifneq ($(CONFIG_SMBD),y)
obj-$(CONFIG_SAMBA4) += icu zlib gmp nettle gnutls samba4
obj-$(CONFIG_SAMBA3) += jansson
obj-$(CONFIG_SAMBA4) += jansson
endif
obj-$(CONFIG_MINIDLNA) += jansson
obj-$(CONFIG_NTFS3G) += ntfs-3g
obj-$(CONFIG_SPEEDTEST_CLI) += curl speedtest-cli zlib
obj-$(CONFIG_RADAUTH) += radauth
ifneq ($(CONFIG_FONERA),y)
ifneq ($(CONFIG_XSCALE),y)
obj-$(CONFIG_MMC) += mmc
else
obj-$(CONFIG_MMC) += mmc-ixp4xx
endif
else
obj-$(CONFIG_MMC) += mmc-fonera
endif
obj-$(CONFIG_ZEROIP) += shat
#obj-$(CONFIG_KAID) += kaid
obj-$(CONFIG_ROBOCFG) += robocfg
obj-$(CONFIG_MULTICAST) += igmp-proxy
obj-$(CONFIG_UDPXY) += udpxy
obj-$(CONFIG_SKYTRON) += skytron
obj-$(CONFIG_OPENVPN) += lzo openvpn
obj-$(CONFIG_OLSRD) += olsrd
obj-$(CONFIG_BATMANADV) += batman-adv
obj-$(CONFIG_FDISK) += fdisk
ifneq ($(CONFIG_MADWIFI),y)
ifneq ($(CONFIG_MADWIFI_MIMO),y)
ifneq ($(CONFIG_RT2880),y)
ifneq ($(CONFIG_RT61),y)
obj-$(CONFIG_NAS) += nas
obj-$(CONFIG_WLCONF) += wlconf
endif
endif
endif
endif
obj-$(CONFIG_UTILS) += utils
obj-$(CONFIG_MTR) += mtr
obj-$(CONFIG_PCIUTILS) += pciutils
obj-$(CONFIG_E2FSPROGS) += lzo e2fsprogs
obj-$(CONFIG_XFSPROGS) += ncurses util-linux xfsprogs 
obj-$(CONFIG_BTRFSPROGS) += ncurses util-linux zstd btrfsprogs
obj-$(CONFIG_HTTPREDIRECT) += http-redirect
obj-$(CONFIG_SMTPREDIRECT) += smtp-redirect
obj-$(CONFIG_SPUTNIK_APD) += sputnik
obj-$(CONFIG_OVERCLOCKING) += overclocking
obj-$(CONFIG_PROXYWATCHDOG) += proxywatchdog
obj-$(CONFIG_JFFS2) += jffs2
obj-$(CONFIG_LANGUAGE) += language
obj-$(CONFIG_NETWORKSETTINGS) += networksettings
obj-$(CONFIG_ROUTERSTYLE) += routerstyle
obj-$(CONFIG_SCHEDULER) += scheduler
obj-$(CONFIG_SYSLOG) += syslog
obj-$(CONFIG_TELNET) += telnet
obj-$(CONFIG_WDSWATCHDOG) += wdswatchdog
obj-$(CONFIG_IPV6) += ipv6 dhcpv6
obj-$(CONFIG_CONNTRACK) += conntrack
obj-$(CONFIG_RADIOOFF) += radiooff
obj-$(CONFIG_PHP) += zlib libzip libgd libpng libxml2 libmcrypt curl icu glib20 sqlite php7 zlib
obj-$(CONFIG_NCURSES) += ncurses
obj-$(CONFIG_IFTOP) += libpcap iftop
obj-$(CONFIG_IPTRAF) += iptraf
obj-$(CONFIG_WIFIDOG) += wifidog
obj-$(CONFIG_HWMON) += hwmon
#obj-$(CONFIG_RSTATS) += rstats
obj-$(CONFIG_STABRIDGE) += stabridge
obj-$(CONFIG_EOP_TUNNEL) += eop-tunnel
obj-$(CONFIG_AIRCRACK) += libnltiny aircrack-ng pcre zlib
obj-$(CONFIG_MOXA) += moxa
obj-$(CONFIG_BONDING) += ifenslave
obj-$(CONFIG_NSTX) += nstx
obj-$(CONFIG_SQUID) += squid
obj-$(CONFIG_IPERF) += iperf
obj-$(CONFIG_NTPD) += ntpd
obj-$(CONFIG_CHRONY) += chrony
obj-$(CONFIG_GPSD) += ncurses gpsd
obj-$(CONFIG_PHP5) += icu zlib glib20 php7
obj-$(CONFIG_FREERADIUS) += libpcap libtalloc freeradius3
#obj-$(CONFIG_FREERADIUS3) += talloc freeradius3
#obj-$(CONFIG_EAD) += ead

obj-$(CONFIG_SCDP) += scdp
obj-$(CONFIG_SES) += ses
obj-$(CONFIG_PRINTER_SERVER) += ippd
obj-$(CONFIG_FTP) += proftpd jansson
obj-$(CONFIG_PCMCIA) += pcmcia 
obj-$(CONFIG_PCMCIA) += microcom
obj-$(CONFIG_PCMCIA) += gcom
obj-$(CONFIG_PCMCIA) += nvtlstatus 
obj-$(CONFIG_PCMCIA) += setserial
obj-$(CONFIG_COMGT) += comgt
obj-$(CONFIG_MEDIASERVER) += mediaserver
obj-$(CONFIG_FRR) += ncurses pcre libyang readline zlib json-c frr libcap
obj-$(CONFIG_QUAGGA) += ncurses readline zlib quagga
obj-$(CONFIG_VPNC) += vpnc
obj-$(CONFIG_STUCK) += stuck_beacon
obj-$(CONFIG_GPSI) += gpsi
obj-$(CONFIG_BMON) += bmon
obj-$(CONFIG_SERCD) += sercd
obj-$(CONFIG_SER2NET) += ser2net
obj-$(CONFIG_STRACE) += libunwind strace
obj-$(CONFIG_RT3062) += rt3062
obj-$(CONFIG_RT2860) += rt2860
obj-$(CONFIG_P910ND) += p910nd
obj-$(CONFIG_DRIVER_WIRED) += libnfnetlink libnetfilter_log
ifneq ($(CONFIG_OPENSSL),y)
obj-$(CONFIG_WPA3) += wolfssl
endif
obj-$(CONFIG_WOLFSSL) += wolfssl
obj-$(CONFIG_HOSTAPD2) += hostapd2
obj-$(CONFIG_WPA_SUPPLICANT2) += wpa_supplicant2
obj-$(CONFIG_MIITOOL) += net-tools
obj-$(CONFIG_TOR) += xz zstd zlib openssl libevent tor
obj-$(CONFIG_RSTP) += rstp
obj-$(CONFIG_OPENLLDP) += openlldp
obj-$(CONFIG_WGETS) += wgets
obj-$(CONFIG_USB) += usb disktype
obj-$(CONFIG_USB_ADVANCED) += sdparm hdparm
obj-$(CONFIG_ASTERISK) += editline zlib ncurses util-linux jansson asterisk
obj-$(CONFIG_ZAPTEL) += zaptel
obj-$(CONFIG_WAVESAT) += wavesat
obj-$(CONFIG_RT2860APD) += rt2860apd apsta_client
obj-$(CONFIG_POUND) += pound
obj-$(CONFIG_VNCREPEATER) += vncrepeater
obj-$(CONFIG_NPROBE) += nprobe
obj-$(CONFIG_MTR) += mtr
obj-$(CONFIG_SNOOP) += snoop
obj-$(CONFIG_AOSS) += aoss
obj-$(CONFIG_AOSS2) += aoss2 json-c libubox ubus
obj-$(CONFIG_AP_SERV) += ap-serv
obj-$(CONFIG_TOLAPAI) += tolapai
obj-$(CONFIG_BUFFALO) += buffalo_flash
#obj-$(CONFIG_RELAYD) += relayd
obj-$(CONFIG_ATH9K) += ath9k 
obj-$(CONFIG_ATH9K) += iw 
obj-$(CONFIG_ATH9K) += crda 
obj-$(CONFIG_ATH9K_DRIVER_ONLY) += libnltiny ath9k iw crda
obj-$(CONFIG_LIBNLTINY) += libnltiny
obj-$(CONFIG_HOTPLUG2) += hotplug2 udev
obj-$(CONFIG_UBOOTENV) += ubootenv
obj-$(CONFIG_DSL_CPE_CONTROL) += dsl_cpe_control atm
obj-$(CONFIG_NLD) += json-c libubox nld
obj-$(CONFIG_NSMD) += json-c nsmd
obj-$(CONFIG_PICOCOM) += picocom
obj-$(CONFIG_OPENDPI) += ndpi-netfilter
obj-$(CONFIG_LLTD) += lltd
obj-$(CONFIG_USBIP) += ncurses util-linux libudev usbip
#obj-$(CONFIG_XTA) += xtables-addons
obj-$(CONFIG_SNORT) += libpcap xz libnfnetlink libnetfilter_queue libdnet daq pcre snort
obj-$(CONFIG_LAGUNA) += gsp_updater
obj-$(CONFIG_VENTANA) += gsp_updater
obj-$(CONFIG_NEWPORT) += gsp_updater
obj-$(CONFIG_POLARSSL) += polarssl
#obj-$(CONFIG_UHTTPD) += cyassl uhttpd pcre lighttpd
obj-$(CONFIG_MSTP) += mstp
obj-$(CONFIG_PYTHON) += zlib libffi python
obj-$(CONFIG_NMAP) += libpcap nmap
obj-$(CONFIG_ARPALERT) += arpalert
obj-$(CONFIG_IPETH) += ipeth
obj-$(CONFIG_IAS) += dns_responder
obj-$(CONFIG_MINIDLNA) += minidlna zlib
obj-$(CONFIG_NRPE) += nrpe
obj-$(CONFIG_LINKS) += links
obj-$(CONFIG_SOFTFLOWD) += softflowd
obj-$(CONFIG_LIGHTTPD) += pcre lighttpd
obj-$(CONFIG_NEXTMEDIAEXTRA) += nextmediaextra
obj-$(CONFIG_LIBQMI) += libffi zlib glib20 libqmi
obj-$(CONFIG_UQMI) += json-c libubox uqmi
obj-$(CONFIG_LIBMBIM) += zlib libffi glib20 libmbim
obj-$(CONFIG_MTDUTILS) += mtd-utils
obj-$(CONFIG_UBIUTILS) += ubi-utils zlib lzo zstd
obj-$(CONFIG_STRONGSWAN) += gmp strongswan sqlite
obj-$(CONFIG_PRIVOXY) += zlib pcre privoxy
obj-$(CONFIG_VENTANA) += kobs-ng
obj-$(CONFIG_WEBSERVER) += libffi zlib libzip openssl glib20 libxml2 libmcrypt lighttpd curl libpng icu sqlite php7 util-linux
obj-$(CONFIG_TRANSMISSION) += libevent curl transmission zlib
obj-$(CONFIG_CLOUD4WI) += curl zlib
obj-$(CONFIG_UNIWIP) += uniwip_gpio
obj-$(CONFIG_MACTELNET) += mactelnet
obj-$(CONFIG_FIRMWARES) += firmwares
obj-$(CONFIG_SERVICEGATE) += servicegate
obj-$(CONFIG_UNBOUND) += unbound
obj-$(CONFIG_JAVA) += java
obj-$(CONFIG_SOFTETHER) += readline softether
obj-$(CONFIG_ALPINE) += qca-ssdk qca-ssdk-shell
obj-$(CONFIG_ETHTOOL) += ethtool
obj-$(CONFIG_ANCHORFREE) += zlib jansson libevent-af hydra
obj-$(CONFIG_F2FS) += f2fs-tools
obj-$(CONFIG_MTDUTILS) += mtd-utils
obj-$(CONFIG_LSOF) += lsof
obj-$(CONFIG_LIBMNL) += libmnl
obj-$(CONFIG_WIREGUARD) += libmnl wireguard qrencode
obj-$(CONFIG_EXFAT) += exfat-utils
obj-$(CONFIG_DOSFSTOOLS) += dosfstools
obj-$(CONFIG_FLASHROM) += flashrom
obj-$(CONFIG_SMARTMONTOOLS) += smartmontools
#obj-$(CONFIG_OPROFILE) += oprofile
ifeq ($(CONFIG_BCMMODERN),y)
obj-$(CONFIG_WPS) += brcmwps
endif
#obj-$(CONFIG_80211AC) += emf
#obj-y+=anchorfree
obj-y+=ttraff
#obj-y+=speedtest
obj-$(CONFIG_MKIMAGE) += mkimage
obj-$(CONFIG_SPEEDCHECKER) +=  json-c libubox speedchecker shownf
obj-$(CONFIG_MIKROTIK_BTEST) += mikrotik_btest
obj-$(CONFIG_BKM) += multisim
obj-$(CONFIG_TMK) += multisim
obj-$(CONFIG_ZFS) += util-linux zlib libtirpc libudev zfs
obj-$(CONFIG_NFS) += lvm2 keyutils libtirpc rpcbind krb5 nfs-utils
obj-$(CONFIG_SCREEN) += ncurses screen
obj-$(CONFIG_DDRESCUE) += ddrescue
obj-$(CONFIG_I2CTOOLS) += i2ctools
obj-$(CONFIG_RAID) += mdadm raidmanager
obj-$(CONFIG_I2C_GPIO_CUSTOM) += i2c-gpio-custom
obj-$(CONFIG_NEWPORT) += cpt8x
obj-$(CONFIG_RSYNC) += openssl zstd rsync
obj-$(CONFIG_CAKE) += cake
obj-$(CONFIG_CAKE) += fq_codel_fast
obj-$(CONFIG_SISPMCTL) += comgt sispmctl
obj-$(CONFIG_APFS) += apfs apfsprogs
obj-$(CONFIG_SMARTDNS) += smartdns
obj-$(CONFIG_NGINX) += pcre openssl zlib nginx
obj-$(CONFIG_X86) += yukon bootconfig
obj-$(CONFIG_MRP) += mrp
obj-$(CONFIG_HTOP) += ncurses libnl htop

obj-y+=configs


obj-configure := $(foreach obj,$(obj-y) $(obj-n),$(obj)-configure)
obj-checkout := $(foreach obj,$(obj-y) $(obj-n),$(obj)-checkout)
obj-update := $(foreach obj,$(obj-y) $(obj-n),$(obj)-update)

ifneq ($(CONFIG_DIST),"micro")
CONFIG_AQOS=y
endif


all:

configs-checkout:
	rm -rf $(TOP)/private
	svn co svn://svn.dd-wrt.com/private $(TOP)/private
	$(TOP)/private/symlinks.sh $(TOP) $(LINUXDIR) $(ARCHITECTURE)
		

configs-update:
#	svn commit -m "faster hand optimized mksquashfs-lzma tool" $(LINUXDIR)
	svn update $(LINUXDIR)
	svn update $(LINUXDIR)/../linux-3.2
	svn update $(LINUXDIR)/../linux-3.5
	svn update $(LINUXDIR)/../linux-3.10
	svn update $(LINUXDIR)/../linux-3.18
	svn update $(LINUXDIR)/../linux-4.4
	svn update $(LINUXDIR)/../linux-4.9
	svn update $(LINUXDIR)/../linux-4.14
	svn update $(TOP)/private
	$(TOP)/private/symlinks.sh $(TOP) $(LINUXDIR) $(ARCHITECTURE)

configs-clean:
	@true

configs-install:
	@true

configs:
	@true
