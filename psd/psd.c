/*
 * Phoenix-RTOS
 *
 * psd - Serial Download Protocol client
 *
 * Copyright 2019 Phoenix Systems
 * Author: Bartosz Ciesla, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdio.h>
#include <usbclient.h>
#include <fcntl.h>
#include <string.h>

#include "hid.h"
#include "sdp.h"


struct {
	int (*rf)(int, char *, unsigned int, char **);
	int (*sf)(int, const char *, unsigned int);

	unsigned int nfiles;
	FILE *f;
	FILE *files[];
} psd;


int psd_readRegister(sdp_cmd_t *cmd)
{
#if 0
	int res, n;
	char buff[SEND_BUF_SIZE] = { 3, 0x56, 0x78, 0x78, 0x56 };
	if ((res = psd.sf(3, buff, 5)) < 0) {
		return -1;
	}

	int offset = 0;
	char* address = cmd->address ? (char *)cmd->address : (char *)psd.devs;
	int size = cmd->datasz * (cmd->format / 8); /* in readRegister datasz means register count not bytes */
	buff[0] = 4;
	while (offset < size) {
		n = (SEND_BUF_SIZE - 1 > size - offset) ? (size - offset) : (SEND_BUF_SIZE - 1);
		memcpy(buff + 1, address + offset, n);
		offset += n;
		if((res = psd.sf(4, buff, n)) < 0) {
			printf("Failed to send register contents\n");
			return res;
		}
	}
#endif
	return EOK;
}


int psd_writeRegister(sdp_cmd_t *cmd)
{
#if 0
	int res;
	char buff[SEND_BUF_SIZE] = { 3, 0x56, 0x78, 0x78, 0x56 };
	if ((res = psd.sf(3, buff, 5)) < 0) {
		return -1;
	}

	if (cmd->address) {
		switch (cmd->format) {
			case 8:
				*((u8 *)cmd->address) = cmd->data & 0xff;
				break;
			case 16:
				*((u16 *)cmd->address) = cmd->data & 0xffff;
				break;
			case 32:
				*((u32 *)cmd->address) = cmd->data;
				break;
			default:
				buff[0] = 4;
				buff[1] = 0x12;
				buff[2] = 0x34;
				buff[3] = 0x34;
				buff[4] = 0x12;
				printf("Failed to write register contents\n");
				return psd.sf(4, buff, SEND_BUF_SIZE);
		}
	} else {
		psd.currDev = cmd->data % psd.devsn;
	}

	buff[0] = 4;
	buff[1] = 0x12;
	buff[2] = 0x8a;
	buff[3] = 0x8a;
	buff[4] = 0x12;
	if((res = psd.sf(4, buff, SEND_BUF_SIZE)) < 0) {
		printf("Failed to send complete status\n");
		return res;
	}
#endif
	return EOK;
}


int psd_writeFile(sdp_cmd_t *cmd)
{
	int res, err = 0;
	offs_t offs, n, l;
	char buff[1025];
	char *outdata;

	if (fseek(psd.f, cmd->address, SEEK_SET) < 0)
		err = -2;

	for (offs = 0, n = 0; !err && (offs < cmd->datasz); offs += n) {

		/* Read data from serial device */		
		if ((res = psd.rf(1, buff, sizeof(buff), &outdata) < 0)) {
			err = -1;
			break;
		}

		/* Write data to file */
		n = min(cmd->datasz - offs, res);
		for (l = 0; l < n;) {	
			if ((res = fwrite(outdata + l, n, 1, psd.f)) < 0) {
				err = -2;
				break;
			}
			l += res;
		}
	}

	/* Handle errors */
	if (!err) {
		buff[0] = 4;
		memset(buff, 0x88, 4);
	}
	else {
		buff[0] = 4;
		memset(buff, 0x88, 4);
	}

	/* Send write status */
	if ((res = psd.sf(buff[0], buff, 5)) < 0)
		err = -3;	

	return err;
}


int main(int argc, char **argv)
{
	char data[11];
	sdp_cmd_t *cmd;

	/* Open files */
	for (psd.nfiles = 1; psd.nfiles < argc; psd.nfiles++) {
		if ((psd.files[psd.nfiles] = fopen(argv[psd.nfiles], "rwb")) == NULL) {
			fprintf(stderr, "Can't open file '%s'!", argv[psd.nfiles]);
			return -1;
		}
	}
	psd.f = psd.files[0];

	printf("Initializing USB transport\n");
	if (hid_init(&psd.rf, &psd.sf)) {
		printf("Couldn't initialize USB transport\n");
		return -1;
	}

	while (1) {
		psd.rf(0, (void *)data, sizeof(cmd) + 1, (void *)&cmd);

		switch (cmd->type) {
			case SDP_READ_REGISTER:
				psd_readRegister(cmd);
				break;
			case SDP_WRITE_REGISTER:
				psd_writeRegister(cmd);
				break;
			case SDP_WRITE_FILE:
				psd_writeFile(cmd);
				break;
		}
	}

	return EOK;
}
