/*
 * bootconfig.c
 *
 * Copyright (C) 2020 Sebastian Gottschall <gottschall@dd-wrt.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id:
 */
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <syslog.h>
#include <signal.h>
#include <services.h>

char *bootconfig_deps(void)
{
	return "boot_disable_msi boot_noaer boot_noari boot_noacpi boot_pcie_tune boot_mds boot_tsx_async_abort boot_srbds boot_nospectre_v1 boot_nospectre_v2 boot_l1tf boot_nospec_store_bypass_disable boot_nopti";
}

void start_bootconfig(void)
{
	char *disc = getdisc();
	char args[512] = { 0 };
	char dev[64];

	insmod("crc16 crc32c_generic crc32_generic mbcache ext2 jbd jbd2 ext3 ext4");

	if (nvram_match("boot_disable_msi", "1"))
		strcat(args, " pci=nomsi");
	if (nvram_match("boot_noaer", "1"))
		strcat(args, " pci=noaer");
	if (nvram_match("boot_noari", "1"))
		strcat(args, " pci=noari");
	if (nvram_match("boot_noacpi", "1"))
		strcat(args, " pci=noacpi");
	if (nvram_match("boot_pcie_tune", "pcie_bus_tune_off"))
		strcat(args, " pci=pcie_bus_tune_off");
	if (nvram_match("boot_pcie_tune", "pcie_bus_safe"))
		strcat(args, " pci=pcie_bus_safe");
	if (nvram_match("boot_pcie_tune", "pcie_bus_perf"))
		strcat(args, " pci=pcie_bus_perf");
	if (nvram_match("boot_pcie_tune", "pcie_bus_peer2peer"))
		strcat(args, " pci=pcie_bus_peer2peer");
	if (nvram_match("boot_mds", "1"))
		strcat(args, " mds=off");
	if (nvram_match("boot_tsx_async_abort", "1"))
		strcat(args, " tsx_async_abort=off");
	if (nvram_match("boot_srbds", "1"))
		strcat(args, " srbds=off");
	if (nvram_match("boot_nospectre_v1", "1"))
		strcat(args, " spectre_v1=off");
	if (nvram_match("boot_nospectre_v2", "1"))
		strcat(args, " spectre_v2=off");
	if (nvram_match("boot_l1tf", "1"))
		strcat(args, " l1tf=off");
	if (nvram_match("boot_nopti", "1"))
		strcat(args, " pti=off");
	if (nvram_match("boot_nospec_store_bypass_disable", "1"))
		strcat(args, " spec_store_bypass_disable=off");
	if (strlen(disc) == 7)	//mmcblk0
		sprintf(dev, "/dev/%sp1", disc);
	else
		sprintf(dev, "/dev/%s1", disc);
	eval("mount", "-t", "ext2", dev, "/boot");
	FILE *in = fopen("/boot/boot/grub/menu.lst", "rb");
	char serial[64];
	fscanf(in, "%s", serial);
	fclose(in);
	FILE *out = fopen("/boot/boot/grub/menu.lst", "wb");
	char *vga = " fbcon=nodefer vga=0x305";
	if (!strncmp(serial, "serial", 6)) {
		fprintf(out, "serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1\n");
		fprintf(out, "terminal --timeout=10 serial\n");
		fprintf(out, "\n");
		vga = "";
	}
	fprintf(out, "default 0\n");
	if (strlen(args)) {
		fprintf(out, "timeout 3\n");
	} else {
		fprintf(out, "timeout 0\n");
	}
	fprintf(out, "\n");
	if (strlen(args)) {
		fprintf(out, "title   DD-WRT\n");
		fprintf(out, "root    (hd0,0)\n");
		fprintf(out, "kernel  /boot/vmlinuz root=/dev/hda2 rootfstype=squashfs noinitrd%s console=tty0,115200n8 reboot=bios rootdelay=5%s\n", vga, args);
		fprintf(out, "boot\n\n");
	}
	fprintf(out, "title   default\n");
	fprintf(out, "root    (hd0,0)\n");
	fprintf(out, "kernel  /boot/vmlinuz root=/dev/hda2 rootfstype=squashfs noinitrd%s console=tty0,115200n8 reboot=bios rootdelay=5\n", vga);
	fprintf(out, "boot\n");
	fprintf(out, "\n");
	fclose(out);
	eval("umount", "/boot");

	return;
}
