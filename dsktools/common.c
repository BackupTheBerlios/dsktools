/*
 * common.c - Common functions for dsktools.
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
 */

#include "common.h"

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
void init_raw_cmd(struct floppy_raw_cmd *raw_cmd)
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

