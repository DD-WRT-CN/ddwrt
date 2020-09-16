#!/usr/bin/env bash
#
# Licensed under the terms of the GNU GPL License version 2 or later.
#
# Author: Peter Tyser <ptyser@xes-inc.com>
#
# U-Boot firmware supports the booting of images in the Flattened Image
# Tree (FIT) format.  The FIT format uses a device tree structure to
# describe a kernel image, device tree blob, ramdisk, etc.  This script
# creates an Image Tree Source (.its file) which can be passed to the
# 'mkimage' utility to generate an Image Tree Blob (.itb file).  The .itb
# file can then be booted by U-Boot (or other bootloaders which support
# FIT images).  See doc/uImage.FIT/howto.txt in U-Boot source code for
# additional information on FIT images.
#

usage() {
	echo "Usage: `basename $0` -C comp [-A arch -a addr -e entry]" \
		"-v version -k kernel -N name [-D name -d dtb] -o its_file"
	echo -e "\t-N ==> set kernel name 'name'"
	echo -e "\t-A ==> set architecture to 'arch'"
	echo -e "\t-C ==> set compression type 'comp'"
	echo -e "\t-a ==> set load address to 'addr' (hex)"
	echo -e "\t-e ==> set entry point to 'entry' (hex)"
	echo -e "\t-v ==> set kernel version to 'version'"
	echo -e "\t-k ==> include kernel image 'kernel'"
	echo -e "\t-D ==> human friendly Device Tree Blob 'name'"
	echo -e "\t-d ==> include Device Tree Blob 'dtb'"
	echo -e "\t-o ==> create output file 'its_file'"
	exit 1
}

# OcteonTX Defaults
ARCH=arm64
LOAD_ADDR=0x20080000
ENTRY_ADDR=0x20080000
while getopts ":A:a:C:D:d:e:k:o:v:N:" OPTION
do
	case $OPTION in
		N ) NAME=$OPTARG;;
		A ) ARCH=$OPTARG;;
		a ) LOAD_ADDR=$OPTARG;;
		C ) COMPRESS=$OPTARG;;
		D ) DEVICE=$OPTARG;;
		d ) DTB=$OPTARG;;
		e ) ENTRY_ADDR=$OPTARG;;
		k ) KERNEL=$OPTARG;;
		o ) OUTPUT=$OPTARG;;
		v ) VERSION=$OPTARG;;
		* ) echo "Invalid option passed to '$0' (options:$@)"
		usage;;
	esac
done

# Make sure user entered all required parameters
if [ -z "${ARCH}" ] || [ -z "${COMPRESS}" ] || [ -z "${LOAD_ADDR}" ] || \
	[ -z "${ENTRY_ADDR}" ] || [ -z "${VERSION}" ] || [ -z "${KERNEL}" ] || \
	[ -z "${OUTPUT}" ]; then
	usage
fi

ARCH_UPPER=`echo $ARCH | tr '[:lower:]' '[:upper:]'`
[ -z "${NAME}" ] && NAME="${ARCH_UPPER} Linux-${VERSION}"

# Conditionally create fdt information
if [ -n "${DTB}" ]; then
	FDT="
		fdt@1 {
			description = \"${NAME} ${DEVICE} device tree blob\";
			data = /incbin/(\"${DTB}\");
			type = \"flat_dt\";
			arch = \"${ARCH}\";
			compression = \"none\";
			hash@1 {
				algo = \"crc32\";
			};
			hash@2 {
				algo = \"sha1\";
			};
		};
"
fi

# Create a default, fully populated DTS file
DATA="/dts-v1/;

/ {
	description = \"${ARCH_UPPER} FIT (Flattened Image Tree)\";
	#address-cells = <1>;

	images {
		kernel@1 {
			description = \"${NAME}\";
			data = /incbin/(\"${KERNEL}\");
			type = \"kernel\";
			arch = \"${ARCH}\";
			os = \"linux\";
			compression = \"${COMPRESS}\";
			load = <${LOAD_ADDR}>;
			entry = <${ENTRY_ADDR}>;
			hash@1 {
				algo = \"crc32\";
			};
			hash@2 {
				algo = \"sha1\";
			};
		};

${FDT}

	};

	configurations {
		default = \"config@1\";
		config@1 {
			description = \"Boot Linux Kernel\";
			kernel = \"kernel@1\";
			fdt = \"fdt@1\";
		};
	};
};"

# Write .its file to disk
echo "$DATA" > ${OUTPUT}
