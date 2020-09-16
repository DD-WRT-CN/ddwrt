//==========================================================================
//
//      net/tftp_server.c
//
//      Stand-alone TFTP server support for RedBoot
//

#include <redboot.h>		// have_net
#include <net/net.h>
#include <net/tftp.h>
#include <net/tftp_support.h>
#include <cyg/io/flash.h>
#include "fwupgrade.h"

#define IPPORT_TFTPD 		69
#define FIRMWARE_PASSWORD 	"flash_update"

#define TFTP_HEADER_SIZE 	4
#define SEGSIZE 		512

/* from main.c */
extern void do_reset(int argc, char *argv[]);

/* global variables for tftpd */
static ip_route_t wrq_route = { ip_addr:{0, 0, 0, 0} };

static word wrq_port = 0;
static char tftpd_buffer[SEGSIZE + TFTP_HEADER_SIZE];

unsigned long tftpd_base = 0;

static char usage[] = "";

// Exported CLI function
RedBoot_cmd("tftpd",
	    "Start tftp server and wait for firmware update", usage, do_tftpd);

static void
tftpd_error(int error_code, char *error_msg, ip_route_t * src_route,
	    word src_port)
{
	struct tftphdr *tp;
	int len;

	diag_printf("TFTPD: Error %d: %s\n", error_code, error_msg);

	tp = (struct tftphdr *)tftpd_buffer;
	tp->th_opcode = htons((unsigned short)ERROR);
	tp->th_code = htons((unsigned short)error_code);
	strcpy(tp->th_msg, error_msg);
	len = strlen(error_msg);
	tp->th_msg[len] = '\0';
	len += (TFTP_HEADER_SIZE + 1);

	__udp_send((char *)tftpd_buffer, len, src_route, src_port,
		   IPPORT_TFTPD);
}

static void
tftpd_send(int opcode, int block, ip_route_t * src_route, word src_port)
{
	struct tftphdr *tp;

#if 0
	diag_printf("tftpd_send\n");
#endif

	tp = (struct tftphdr *)tftpd_buffer;
	tp->th_opcode = htons((unsigned short)opcode);
	tp->th_block = htons((unsigned short)block);

	__udp_send((char *)tftpd_buffer, TFTP_HEADER_SIZE, src_route, src_port,
		   IPPORT_TFTPD);
}

#if 0
/* jM -- set status led green or red */
#define STATUS_LED_GPIO 2
static void set_status_led(int status)
{
	HAL_GPIO_OUTPUT_ENABLE(STATUS_LED_GPIO);

	if (status == 0)
		HAL_GPIO_OUTPUT_CLEAR(STATUS_LED_GPIO);
	else
		HAL_GPIO_OUTPUT_SET(STATUS_LED_GPIO);
}
#endif

#include "../ramconfig.h"

extern int page_programming_supported;
extern int page_gpio;

struct firmware_formats {
	char *name;
	int (*fw_check_image) (unsigned char *, unsigned long, int);
};

static const struct firmware_formats fw_formats[] = {
	{.name = "DD-WRT",.fw_check_image = fw_check_image_ddwrt},
	{.name = "UBIQUITI",.fw_check_image = fw_check_image_ubnt},
#ifndef CYGPKG_HAL_MIPS_AR7100
	{.name = "WILIGEAR",.fw_check_image = fw_check_image_wili},
	{.name = "SENAO",.fw_check_image = fw_check_image_senao},
#endif
};

static void do_flash_update(unsigned long base_addr, unsigned long len,
			    int index)
{
	int rc;
#if defined(CYGPKG_HAL_MIPS_AR2316)
#if FISTYPE == 1
	set_gpio(0, 1);
	page_programming_supported = 1;
	page_gpio = 0;
#endif

#if FISTYPE == 2
	set_gpio(3, 1);
	page_programming_supported = 1;
	page_gpio = 3;
#endif

#if FISTYPE == 0
	page_programming_supported = 0;
	page_gpio = 0;
#endif
#endif
	/* do_flash = 1, write to flash */
	rc = fw_formats[index].fw_check_image((char *)base_addr, len, 1);
	if (rc)
		diag_printf("Flash update failed!\n");
	else
		diag_printf("Flash update complete.\n");
	/* clean da memory */
	memset((unsigned char *)base_addr, 0, len);
	do_reset(0, NULL);
}

void
tftpd_fsm(struct tftphdr *tp, int len, ip_route_t * src_route, word src_port)
{
	char *mode, *password;
	static unsigned int last_sync_block;
	unsigned int block;
	static int bytes_received = 0;
	static unsigned long ptr = 0L;

#if 0
	diag_printf("got packet from %d.%d.%d.%d port %d\n",
		    src_route->ip_addr[0],
		    src_route->ip_addr[1],
		    src_route->ip_addr[2], src_route->ip_addr[3], src_port);

	diag_printf("packet len is %d, TFTP opcode is %d\n",
		    len, tp->th_opcode);
#endif

	switch (tp->th_opcode) {
	case WRQ:		/* write request */
		password = tp->th_stuff;
		mode = &password[strlen(password) + 1];
		diag_printf("TFTPD: Connect from %d.%d.%d.%d port %d\n",
			    src_route->ip_addr[0],
			    src_route->ip_addr[1],
			    src_route->ip_addr[2],
			    src_route->ip_addr[3], src_port);
		//if (strcmp(password, FIRMWARE_PASSWORD)) {
		//      tftpd_error(EACCESS, "Access violation (invalid password)", src_route, src_port);
		//      break;
		//}
		if (strcmp(mode, "octet")) {
			tftpd_error(EACCESS,
				    "Access violation (use binary mode)",
				    src_route, src_port);
			break;
		}

		/* we are ready for data */
		tftpd_send(ACK, 0, src_route, src_port);

		last_sync_block = 0;
		memmove(&wrq_route.ip_addr, &src_route->ip_addr,
			sizeof(ip_addr_t));
		wrq_port = src_port;

		break;

	case DATA:		/* received data */
		if (*(int *)(wrq_route.ip_addr) != *(int *)(src_route->ip_addr))
			tftpd_error(EACCESS,
				    "Access violation (unknown address)",
				    src_route, src_port);

		block = ntohs(tp->th_block);
		len -= TFTP_HEADER_SIZE;

		if (block == 1) {
			bytes_received = 0;
			ptr = BASE_ADDR;	/* rewind pointer */

		}
		bytes_received += len;
#if 0
		if (!(bytes_received % (1024 * 16)))
			diag_printf("ptr = %08X\n", ptr);
#endif
		if (bytes_received >= MAX_IMAGE_SIZE) {
			tftpd_error(EACCESS, "Image size too big.", src_route,
				    src_port);
			break;
		}
		if (block == last_sync_block + 1) {
			last_sync_block = block;

			memmove((char *)ptr, &tp->th_data, len);
			ptr += len;

		} else {
			/* out of sync */
			diag_printf("Blocks out of sync (prev %d, curr %d)",
				    last_sync_block, block);
			break;
		}

		if (len < SEGSIZE) {
			diag_printf("TFTPD: Upload completed (got %d bytes).\n",
				    ptr - BASE_ADDR);

			// CRC CHECK
			diag_printf("Checking uploaded file...\n");

			int detect = -1;
			int i;
			for (i = 0;
			     i <
			     sizeof(fw_formats) /
			     sizeof(struct firmware_formats); i++) {
				int v = fw_formats[i].fw_check_image((char *)
								     BASE_ADDR,
								     ptr -
								     BASE_ADDR,
								     0);
//                              diag_printf("%s returns %d\n",fw_formats[i].name,v);
				if (!v) {
					detect = i;
					break;
				}
			}

			if (detect != -1)	/* third parameter 0 - do not write to flash */
				tftpd_send(ACK, block, src_route, src_port);	// crc ok
			else {
				tftpd_error(EACCESS, "CRC error", src_route,
					    src_port);
				break;
			}

			// write to flash       
#if 1
			diag_printf("%s: firmware format detected\n",
				    fw_formats[detect].name);
			do_flash_update(BASE_ADDR, ptr - BASE_ADDR, detect);
#else
			diag_printf("Not writing to flash.\n");
#endif

		} else {
			/* send ACK to last block in sequence */
			tftpd_send(ACK, block, src_route, src_port);
		}
		break;
	default:
		tftpd_error(EBADOP, "Illegal TFTP operation", src_route,
			    src_port);
		break;
	}
}

void
tftpd_handler(udp_socket_t * skt, char *buf, int len,
	      ip_route_t * src_route, word src_port)
{
	struct tftphdr *tp;

#if 0
	diag_printf("tftpd handler called!\n");
#endif

	tp = (struct tftphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	tftpd_fsm(tp, len, src_route, src_port);

}

void do_tftpd(int argc, char *argv[])
{
	udp_socket_t udp_skt;

	/* First, fwupdate.bin will be loaded to BASE_ADDR.
	 * Then before writing to flash each partition (kernel, ramdisk)
	 * will be moved to FW_TEMP_BASE (thus this address will be in FIS mem entry
	 */
	BASE_ADDR =
	    ((CYG_ADDRWORD) mem_segments[0].start + 0x03ff) & ~0x03ff;
	/* note: memory addresses from 80700000 are reserved ? anyway, do not use them  */
	if ((BASE_ADDR + MAX_IMAGE_SIZE) >= 0x80730000)
		diag_printf
		    ("Warning: memory buffer for uploaded file may be on reserved RAM area.\n");

#if 1
	/* hack: on ar531x, elf image should always be at 0x80002000 base, so adjust if possible */
//	if ((FW_TEMP_BASE < 0x80002000)
//	    && (FW_TEMP_BASE + MAX_PART_SIZE <= BASE_ADDR))
//		FW_TEMP_BASE = 0x80002000;
#endif

	diag_printf
	    ("TFTPD is running (using memory ranges: %p - %p).\n",
	     BASE_ADDR, BASE_ADDR + MAX_IMAGE_SIZE);

	__udp_install_listener(&udp_skt, IPPORT_TFTPD, tftpd_handler);

	while (!_rb_break(0)) {
		__enet_poll();	// wait for packet ?
	}

	__udp_remove_listener(IPPORT_TFTPD);
	diag_printf("TFTPD terminated.\n");
}
