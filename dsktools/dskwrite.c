/*
 * dskwrite.c - Small utility to write CPC disk images to a floppy disk under
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
 * V0.0.1 20.6.2001:
 * - first working version
 * V0.0.2 21.6.2001:
 * - added rudimentary support for EDSK images
 * - added command line argument for input file instead of reading stdin
 * V0.0.3 21.12.2001:
 * - merged in changes from Kevin Thacker to handle more copy protection
 *   schemes: - invalid track and head ids in sector headers
 *            - deleted data (untested)
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

/* These raw floppy commands are missing in fdreg.h. Use with caution.
 */
#define FD_READ_DEL		0xCC	/* read deleted with MT, MFM */
#define FD_WRITE_DEL		0xC9	/* write deleted with MT, MFM */

/* Boolean values
 */
#define	TRUE -1
#define	FALSE 0

/* Various DSK image file and actual disk parameters
 */
#define MAGIC_DISK "MV - CPC"
#define MAGIC_EDISK "EXTENDED"
#define	TRACKS 40
#define HEADS 1
#define TRACKLEN_INFO (TRACKLEN + 0x100)

#define MAGIC_TRACK "Track-Info"
#define HEAD 0
#define BPS 2
#define SPT 9
#define GAP 0x4E
#define FILL 0xE5
#define ERR1 0
#define ERR2 0

#define TRACKLEN 0x1200

#define MAX_TRACKLEN 0x2000

#define OFF_IBM 0x01
#define OFF_SYS 0x41
#define OFF_DAT 0xC1


typedef struct diskinfo_t {
	char magic[0x22];
	unsigned char unused1[0x0E];
	unsigned char tracks;
	unsigned char heads;
	unsigned char tracklen[0x02];
	unsigned char tracklenhigh[0xCC];
} Diskinfo;

typedef struct sectorinfo_t {
	unsigned char track;
	unsigned char head;
	unsigned char sector;
	unsigned char bps;
	unsigned char err1;
	unsigned char err2;
	unsigned char unused1;
	unsigned char unused2;
} Sectorinfo;

typedef struct trackinfo_t {
	char magic[0x0D];
	unsigned char unused1[0x03];
	unsigned char track;
	unsigned char head;
	unsigned char unused2[0x02];
	unsigned char bps;
	unsigned char spt;
	unsigned char gap;
	unsigned char fill;
	Sectorinfo sectorinfo[29];
} Trackinfo;

/* format map */
typedef	struct format_map {
	unsigned char cylinder;
	unsigned char head;
	unsigned char sector;
	unsigned char size;
} format_map_t;

void myabort(char *s)
{
	fprintf(stderr,s);
	exit(1);
}

void printdiskinfo(FILE *out, Diskinfo *diskinfo)
{
	char *magic = diskinfo->magic;
	int tracks = diskinfo->tracks;
	int heads = diskinfo->heads;
	int tracklen = diskinfo->tracklen[0] + (diskinfo->tracklen[1] * 256);
	fprintf(out, "MAGIC:\t%s\nTRACKS:\t%i\nHEADS:\t%i\nTRACKL:\t%X\n",
		magic, tracks, heads, tracklen);
}

void printsectorinfo(FILE *out, Sectorinfo *sectorinfo)
{
	fprintf(out, "%X:%i ", sectorinfo->sector, sectorinfo->bps);
}

void printtrackinfo(FILE *out, Trackinfo *trackinfo)
{
	int i;
	char *magic = trackinfo->magic;
	int track = trackinfo->track;
	int head = trackinfo->head;
	int bps = trackinfo->bps;
	int spt = trackinfo->spt;
	int gap = trackinfo->gap;
	int fill = trackinfo->fill;
	/*fprintf(out, "MAGIC:\t%sTRACK: %2.2i HEAD: %i BPS: %i SPT: %i GAP: 0x%X FILL: 0x%X\n",
		magic, track, head, bps, spt, gap, fill);*/
	/*fprintf(out, "TRACK: %2.2i HEAD: %i BPS: %i SPT: %i GAP: 0x%X FILL: 0x%X\n",
		track, head, bps, spt, gap, fill);*/
	fprintf(out, "%2.2i: %i-%i-%i-%X-%X",
		track, head, bps, spt, gap, fill);
	/*for (i=0; i<trackinfo->spt; i++) {
		printsectorinfo(out, trackinfo->sectorinfo+i);
	}
	fprintf(out,"\n");*/
}

void init_sectorinfo(Sectorinfo *sectorinfo, int track, int head, int sector)
{
	sectorinfo->track = track;
	sectorinfo->head = head;
	sectorinfo->sector = sector;
	sectorinfo->bps = BPS;
	sectorinfo->err1 = 0;
	sectorinfo->err2 = 0;
	sectorinfo->unused1 = 0;
	sectorinfo->unused2 = 0;
}

/* Initialise a raw FDC command */
static void init_raw_cmd(struct floppy_raw_cmd *raw_cmd)
{
	raw_cmd->flags = 0;
	raw_cmd->track = 0;
	raw_cmd->data  = NULL;
	raw_cmd->kernel_data = NULL;
	raw_cmd->next  = NULL;
	raw_cmd->length = 0;
	raw_cmd->phys_length = 0;
	raw_cmd->buffer_length = 0;
	raw_cmd->cmd_count = 0;
	raw_cmd->reply_count = 0;
	raw_cmd->resultcode = 0;	
}

/* notes:
 *
 * the C (track),H (head),R (sector id),N (sector size) parameters in the
 * sector id field do not need to be the same as the physical track and
 * physical side.
 */

void format_track(int fd, int track, Trackinfo *trackinfo) {

	int i, err;
	struct floppy_raw_cmd raw_cmd;
	format_map_t data[20];		//FIXME
	unsigned char mask = 0xFF;
	Sectorinfo *sectorinfo;

	sectorinfo = trackinfo->sectorinfo;
	for (i=0; i<trackinfo->spt; i++) {
		//data[i].sector = 0xC1+i;
		//data[i].size = 2;	/* 0=128, 1=256, 2=512,... */
		data[i].sector = sectorinfo->sector;
		data[i].size = sectorinfo->bps;
		data[i].cylinder = sectorinfo->track;
		data[i].head = sectorinfo->head;	
		sectorinfo++;
	}
	//fprintf(stderr, "Formatting Track %i\n", track);
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
	raw_cmd.flags |= FD_RAW_NEED_SEEK;
	raw_cmd.track = track;
	raw_cmd.rate  = 2;	/* SD */
	//raw_cmd.length= 512;	/* Sectorsize */
	raw_cmd.length= (128<<(trackinfo->bps));
	raw_cmd.data  = data;

	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_FORMAT & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = 0;	/* head: 4 or 0 */	//FIXME - this is the physical side to read from
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 2;	/* sectorsize */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 9;	/* sectors */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 82;/* GAP */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 0;	/* filler */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->bps;	/* sectorsize */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->spt;	/* sectors */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->gap;	/* GAP */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->fill;	/* filler */
	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		perror("Error formatting");
		exit(1);
	}
	if (raw_cmd.reply[0] & 0x40) {
		fprintf(stderr, "Could not format track %i\n", track);
		exit(1);
	}
}

/* notes:
 *
 * when writing, you must specify the sector c,h,r,n exactly, otherwise fdc
 * will fail to write data to sector.
 */

//void write_sect(int fd, int track, unsigned char sector, unsigned char *data) {
void write_sect(int fd, Trackinfo *trackinfo, Sectorinfo *sectorinfo,
	unsigned char *data) {

	int i, err;
	struct floppy_raw_cmd raw_cmd;
	//format_map_t data[9];
	unsigned char mask = 0xFF;

	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
	raw_cmd.flags |= FD_RAW_NEED_SEEK;
	//raw_cmd.track = track;
	//raw_cmd.rate  = 2;	/* SD */
	//raw_cmd.length= 512;	/* Sectorsize */
	//raw_cmd.data  = data;
	raw_cmd.track = sectorinfo->track;
	raw_cmd.rate  = 2;	/* SD */
	raw_cmd.length= (128<<(sectorinfo->bps));
	raw_cmd.data  = data;

	//raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE & mask;
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 0;		/* head */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = track;	/* track */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 0;		/* head */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = sector;	/* sector */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 2;		/* sectorsize */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = sector;	/* sector */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 82;	/* GPL */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 0xFF;	/* DTL */

	if (sectorinfo->unused1 & 0x040)
	{
		/* "write deleted data" (totally untested!) */
		raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE_DEL & mask;
	}
	else
	{
		/* "write data" */
		raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE & mask;
	}

	// these parameters are same for "write data" and "write deleted data".
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
		perror("Error writing");
		exit(1);
	}
	if (raw_cmd.reply[0] & 0x40) {
		fprintf(stderr, "Could not write sector %0X\n",
			sectorinfo->sector);
	}
}

void writedsk(char *filename) {

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
	in = fopen(filename, "r");
	if (in == NULL) {
		perror("Error opening image file");
		exit(1);
	}

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

}

int main(int argc, char **argv) {

	if (argc == 2) { writedsk(argv[1]);
	} else { fprintf(stderr, "usage: dskwrite <filename>\n");
	}
	return 0;

}

