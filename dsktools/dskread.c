/*
 * dskread.c - Small utility to read CPC disk images from a floppy disk under
 * Linux with a standard PC FDC.
 * Copyright (C)2001 Andreas Micklei <nurgle@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * V0.0.4 24.12.2001:
 * - First version of dskread in dsktools.
 *
 * TODO:
 * - support EDSK properly
 * - handle less common parameter like double sided disks, etc.
 * - handle difficult copy protection schemes like the one on Prehistoric2 for
 *   example
 * - make side of disc (head) selectable
 * - improve user interface
 * - do tool for reading floppys into images
 * - clean up code
 * - split code into separate modules/files
 * - integrate with amssdsk
 * - add clever build system, use GNU autoconf/automake
 * - maybe sync with John Elliots libdsk
 * - build GTK+ GUI
 */

#include "common.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>

void read_sect(int fd, Trackinfo *trackinfo, Sectorinfo *sectorinfo,
	unsigned char *data) {

	int i, err;
	struct floppy_raw_cmd raw_cmd;
	unsigned char mask = 0xFF;

	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
	raw_cmd.flags |= FD_RAW_NEED_SEEK;
	raw_cmd.track = sectorinfo->track;
	raw_cmd.rate  = 2;	/* SD */
	raw_cmd.length= (128<<(sectorinfo->bps));
	raw_cmd.data  = data;

	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_READ & mask;

	raw_cmd.cmd[raw_cmd.cmd_count++] = 0;			/* head */	//FIXME - this is the physical side to read from
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->track;	/* track */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->head;	/* head */	
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->sector;	/* sector */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->bps;	/* sectorsize */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->sector;	/* sector */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->gap;	/* GPL */
	raw_cmd.cmd[raw_cmd.cmd_count++] = 0xFF;		/* DTL */

	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		perror("Error reading");
		exit(1);
	}
	if (raw_cmd.reply[0] & 0x40) {
		fprintf(stderr, "Could not read sector %0X\n",
			sectorinfo->sector);
	}
}

void readdsk(char *filename) {

	/* Variable declarations */
	int fd, tmp, err;
	char *drive;
	struct floppy_raw_cmd raw_cmd;
	//char buffer[ 512 * 2 * 24 ];
	//char buffer[ 512 * 9 ];
	char buffer[ 9 * sizeof(format_map_t) ];
	format_map_t *data;

	Diskinfo diskinfo;
	Trackinfo trackinfo;
	Sectorinfo *sectorinfo, **sectorinfos;
	unsigned char track[TRACKLEN], *sect;
	int tracklen;
	FILE *in;
	int i, j, count;
	char *magic_disk = MAGIC_DISK;
	char *magic_edisk = MAGIC_EDISK;
	char *magic_track = MAGIC_TRACK;
	char flag_edisk = FALSE;	// indicates extended disk image format

	/* initialization */
	drive = "/dev/fd0";

	/* open drive */
	fd = open( drive, O_ACCMODE | O_NDELAY);
	if ( fd < 0 ){
		perror("Error opening floppy device");
		exit(1);
	}

	/* open file */
	in = fopen(filename, "w");
	if (in == NULL) {
		perror("Error opening image file");
		exit(1);
	}

	// FIXME: All wrong after here

#if 0
	/* read disk info, detect extended image */
	count = fread(&diskinfo, 1, sizeof(diskinfo), in);
	if (count != sizeof(diskinfo)) {
		myabort("Error reading Disk-Info: File to short\n");
	}
	if (strncmp(diskinfo.magic, magic_disk, strlen(magic_disk))) {
		if (strncmp(diskinfo.magic, magic_edisk, strlen(magic_edisk))) {
			myabort("Error reading Disk-Info: Invalid Disk-Info\n");
		}
		flag_edisk = TRUE;
	}
	printdiskinfo(stderr, &diskinfo);

	/* Get tracklen for normal disk images */
	tracklen = (diskinfo.tracklen[0] + diskinfo.tracklen[1]*256) - 0x100;

	/*fprintf(stderr, "writing Track: ");*/
	for (i=0; i<diskinfo.tracks; i++) {
		/* read in track */
		/*fprintf(stderr, "%2.2i ",i);
		fflush(stderr);*/
		if (flag_edisk) tracklen = diskinfo.tracklenhigh[i]*256 - 0x100;
		if (tracklen > MAX_TRACKLEN) {
			myabort("Error: Track to long.\n");
		}

		/* read trackinfo */
		memset(&trackinfo, 0, sizeof(trackinfo));
		count = fread(&trackinfo, 1, sizeof(trackinfo), in);
		if (count != sizeof(trackinfo)) {
			myabort("Error reading Track-Info: File to short\n");
		}
		if (strncmp(trackinfo.magic, magic_track, strlen(magic_track)))
			myabort("Error reading Track-Info: Invalid Track-Info\n");

		printtrackinfo(stderr, &trackinfo);

		/* read track */
		count = fread(track, 1, tracklen, in);
		if (count != tracklen)
			myabort("Error reading Track: File to short\n");

		/* format track */
		format_track(fd, i, &trackinfo);

		/* write track */
		sect = track;
		sectorinfo = trackinfo.sectorinfo;
		fprintf(stderr, " [");
		for (j=0; j<trackinfo.spt; j++) {
			fprintf(stderr, "%0X ", sectorinfo->sector);
			write_sect(fd, &trackinfo, sectorinfo, sect);
			sectorinfo++;
			sect += (128<<trackinfo.bps);
		}
		fprintf(stderr, "]\n");
	}
	fprintf(stderr,"\n");
#endif

}

int main(int argc, char **argv) {

	if (argc == 2) { readdsk(argv[1]);
	} else { fprintf(stderr, "usage: dskread <filename>\n");
	}
	return 0;

}

