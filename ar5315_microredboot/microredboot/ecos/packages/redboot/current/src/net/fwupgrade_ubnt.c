/*
firmware upgrade code for UBNT images
huge parts are taken from ubnt fwsplit utility

2009, Sebastian Gottschall
*/

#include <redboot.h>
#include <cyg/io/flash.h>
#include <fis.h>
#include <flash_config.h>
#include "fwupgrade_ubnt.h"

/* some variables from flash.c */
extern void *flash_start, *flash_end;
extern int flash_block_size, flash_num_blocks;
#ifdef CYGOPT_REDBOOT_FIS
extern void *fis_work_block;
extern void *fis_addr;
extern int fisdir_size;		// Size of FIS directory.
#endif
//extern void _show_invalid_flash_address(CYG_ADDRESS flash_addr, int stat); 
extern void fis_update_directory(void);

//#define TRACE diag_printf("DBG: %s:%d\n", __FUNCTION__, __LINE__)

#define TRACE

#define MAX_PARTS 8

typedef struct fw_part {
	part_t *header;
	unsigned char *data;
	u_int32_t data_size;
	part_crc_t *signature;
} fw_part_t;

typedef struct fw {
	u_int32_t size;
	char version[256];
	fw_part_t parts[MAX_PARTS];
	int part_count;
} fw_t;

extern void addPartition(char *name, unsigned int flashbase,
			 unsigned int memaddr, unsigned int entryaddr,
			 unsigned int partsize, unsigned int datasize);

#define crc32 cyg_ether_crc32_accumulate

extern void fis_init(int argc, char *argv[], int force);

int fw_check_image_ubnt(unsigned char *addr, unsigned long maxlen, int do_flash)
{
	header_t *header = (header_t *) addr;
	fw_t fw;
	int len = sizeof(header_t) - (2 * sizeof(u_int32_t));
	unsigned int crc = crc32(0L, (unsigned char *)header, len);
	signature_t *sig;
	fw.size = maxlen;
	if (!strncmp(header->magic, "OPEN", 4)
	    && !strncmp(header->magic, "UBNT", 4)) {
		return -1;	//not ubnt firmware
	}
#if defined(CYGPKG_HAL_MIPS_AR2316)
	if (strncmp((unsigned char *)&header->version[4], "ar2316", 6)) {
		diag_printf
		    ("UBNT_FW: cannot upgrade, wrong target platform. only ar2316 is valid");
		return -1;
	}
#endif
#if defined(CYGPKG_HAL_MIPS_AR5312)
	if (strncmp((unsigned char *)&header->version[4], "ar2313", 6)) {
		diag_printf
		    ("UBNT_FW: cannot upgrade, wrong target platform. only ar2313 is valid");
		return -1;
	}
#endif
#if defined(CYGPKG_HAL_MIPS_AR7100)
	if (strncmp((unsigned char *)&header->version[4], "ar7100", 6) && strncmp((unsigned char *)&header->version[6], "ar7100pro", 9)) {
		diag_printf
		    ("UBNT_FW: cannot upgrade, wrong target platform. only ar7100 (RouterStation) or ar7100pro (RouterStation PRO) is valid");
		return -1;
	}
#endif
	if (htonl(crc) != header->crc) {
		diag_printf("UBNT_FW: header crc failed\n");
		return -1;
	}
	memcpy(fw.version, header->version, sizeof(fw.version));
	if (do_flash)
		diag_printf("UBNT_FW: Firmware version: '%s'\n",
			    header->version);
	part_t *p;
	p = (part_t *) (addr + sizeof(header_t));
	int i = 0;
	while (strncmp(p->magic, MAGIC_END, MAGIC_LENGTH) != 0) {
		if (do_flash) {
			diag_printf("Partition: %s [%u]\n", p->name,
				    ntohl(p->index));
			diag_printf("Partition size: 0x%X\n",
				    ntohl(p->part_size));
			diag_printf("Data size: %u\n", ntohl(p->data_size));
		}
		if ((strncmp(p->magic, MAGIC_PART, MAGIC_LENGTH) == 0)
		    && (i < MAX_PARTS)) {
			fw_part_t *fwp = &fw.parts[i];

			fwp->header = p;
			fwp->data = (unsigned char *)p + sizeof(part_t);
			fwp->data_size = ntohl(p->data_size);
			fwp->signature =
			    (part_crc_t *) (fwp->data + fwp->data_size);
			crc =
			    htonl(crc32
				  (0L, (unsigned char *)p,
				   fwp->data_size + sizeof(part_t)));
			if (crc != fwp->signature->crc) {
				diag_printf
				    ("UBNT_FW: Invalid '%s' CRC (claims: %u, but is %u)\n",
				     fwp->header->name, fwp->signature->crc,
				     crc);
				return -1;
			}
			++i;
		}

		p = (part_t *) ((unsigned char *)p + sizeof(part_t) +
				ntohl(p->data_size) + sizeof(part_crc_t));

		/* check bounds */
		if (((unsigned char *)p - addr) >= maxlen) {
			return -3;
		}
	}

	fw.part_count = i;

	sig = (signature_t *) p;
	if (strncmp(sig->magic, MAGIC_END, MAGIC_LENGTH) != 0) {
		diag_printf("UBNT_FW: Bad firmware signature\n");
		return -4;
	}

	crc = htonl(crc32(0L, addr, (unsigned char *)sig - addr));
	if (crc != sig->crc) {
		diag_printf
		    ("UBNT_FW: Invalid signature CRC (claims: %u, but is %u)\n",
		     sig->crc, crc);
		return -5;
	}

	if (do_flash) {
		char *arg[] = { "fis", "init" };
		fis_init(2, arg, 1);
		void *err_addr;
		flash_read(fis_addr, fis_work_block, fisdir_size,
			   (void **)&err_addr);
		for (i = 0; i < fw.part_count; ++i) {
			fw_part_t *fwp = &fw.parts[i];
			/* do not flash bootloaders bigger than 64kb, since it makes no sense to step back */
			if (!strncmp(fwp->header->name, "RedBoot", 7)
			    && ntohl(fwp->header->part_size) > 0x10000) {
				diag_printf("ignore %s\n", fwp->header->name);
				continue;
			}
			diag_printf("UBNT_FW: Flashing: %s\n",
				    fwp->header->name);
			int stat;

			unsigned int base = ntohl(fwp->header->baseaddr);
			/* convert flash mappings to fit to the current bootloader flash mapping which might be incompatible */
#if defined(CYGPKG_HAL_MIPS_AR7100)
			if ((base & 0xbf00000) == 0xbf00000) {
				base ^= 0xbf00000; // for AR7100
			}
#else
			if ((base & 0xbfc00000) == 0xbfc00000) {
				base ^= 0xbfc00000;// mips mapping 
			} else if ((base & 0xbe00000) == 0xbe00000) {
				base ^= 0xbe00000; // for AR5312 
			} else if ((base & 0xa800000) == 0xa800000) {
				base ^= 0xa800000; // for AR5315 8 MB mapping
			}
#endif
			base |= CYGNUM_FLASH_BASE;

			stat = erase_and_flash("UBNT_FW",base,(void *)(void *)fwp->data,ntohl(fwp->header->part_size));

			addPartition(fwp->header->name, base,
				     ntohl(fwp->header->memaddr),
				     ntohl(fwp->header->entryaddr),
				     ntohl(fwp->header->part_size),
				     ntohl(fwp->header->data_size));
		}
		addPartition("cfg",
			     ((unsigned int)flash_end + 1) -
			     (flash_block_size * 3), 0, 0, 0x10000, 0x10000);

		fis_update_directory();
		diag_printf("UBNT_FW: flashing done\n");
	}

	return 0;
}
