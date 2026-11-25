/*
 *  rsrc_trace.cpp - log the resources the Mac loads (MACEMU_TRACE_RSRC)
 *
 *  Basilisk II (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  MACEMU_TRACE_RSRC=path   writes one tab-separated line per resource the
 *                           Resource Manager hands out ("-" for stderr)
 *  MACEMU_TRACE_RSRC_ALL=1  every call; without it each resource of each
 *                           file is written once, the first time
 *
 *  The resource is identified by its handle, not by the type the patch
 *  passes: the handle is looked up in the reference lists of every open
 *  resource map (the chain from TopMapHndl), which gives the real type, ID,
 *  name, attributes, offset in the resource fork's data area and the file.
 *  Layouts: Inside Macintosh: More Macintosh Toolbox, 1-121 ff. (resource
 *  map in memory) and Files, 2-81 (FCB).
 */

#include "sysdeps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <string>

#include "cpu_emulation.h"
#include "rsrc_trace.h"

// Low-memory globals
const uint32 TopMapHndl = 0xa50;	// handle to the most recently opened resource map
const uint32 FCBSPtr = 0x34e;		// pointer to the file control block buffer
const uint32 Ticks = 0x16a;			// ticks since startup
const uint32 FSFCBLen = 0x3f6;		// length of an FCB (-1 on MFS)
const uint32 VCBQHdr = 0x356;		// queue of the mounted volumes

// Resource map in memory
const uint32 MAP_NEXT = 16;			// handle to the next map
const uint32 MAP_REFNUM = 20;		// file reference number
const uint32 MAP_TYPE_LIST = 24;	// offset to the type list (from the map)
const uint32 MAP_NAME_LIST = 26;	// offset to the name list (from the map)
const uint32 REF_ENTRY_SIZE = 12;	// ID, name offset, attributes + data offset, handle
const int16 CLASSIC_FCB_LENGTH = 94;	// an HFS FCB in System 7's table
const uint32 FCB_VPTR = 20;			// fcbVPtr, the volume's VCB
const uint32 FCB_CNAME = 62;		// fcbCName, the file name (Str31)

static bool initialised = false;
static FILE *trace_file = NULL;
static bool trace_all = false;
static std::set<std::string> seen;	// file, type, ID of resources already written

// Mac address range check, so a damaged map never makes us read outside the
// Mac's memory (Mac2HostAddr does no checking).
static bool valid(uint32 address, uint32 length)
{
#ifdef SHEEPSHAVER
	const uint32 ram_end = RAMBase + RAMSize;
	const uint32 rom_start = ROMBase, rom_end = ROMBase + ROM_SIZE;
#else
	const uint32 ram_end = RAMBaseMac + RAMSize;
	const uint32 rom_start = ROMBaseMac, rom_end = ROMBaseMac + ROMSize;
#endif
	if (address + length < address)
		return false;
	if (address + length <= ram_end)
		return true;
	return address >= rom_start && address + length <= rom_end;
}

static std::string four_char_code(uint32 code)
{
	std::string text;
	for (int shift = 24; shift >= 0; shift -= 8) {
		char c = (char)((code >> shift) & 0xff);
		if (c == '\t' || c == '\n' || c == '\\')
			text += '\\', text += (c == '\t' ? 't' : c == '\n' ? 'n' : '\\');
		else if ((uint8)c < 0x20 || (uint8)c == 0x7f)
			text += '?';
		else
			text += c;	// MacRoman bytes are written as they are
	}
	return text;
}

// A Pascal string at a Mac address, tabs and newlines escaped; "" when unreadable.
static std::string pascal_string(uint32 address)
{
	if (!valid(address, 1))
		return "";
	uint32 length = ReadMacInt8(address);
	if (!valid(address + 1, length))
		return "";
	std::string text;
	for (uint32 i = 0; i < length; i++) {
		uint8 c = ReadMacInt8(address + 1 + i);
		if (c == '\t') text += "\\t";
		else if (c == '\n' || c == '\r') text += "\\n";
		else if (c == '\\') text += "\\\\";
		else text += (char)c;
	}
	return text;
}

// Is vcb one of the volumes in the VCB queue?
static bool known_volume(uint32 vcb)
{
	uint32 entry = ReadMacInt32(VCBQHdr + 2);	// qHead
	for (int volumes = 0; volumes < 256 && entry != 0 && valid(entry, 4); volumes++) {
		if (entry == vcb)
			return true;
		entry = ReadMacInt32(entry);	// qLink
	}
	return false;
}

// The file name of a file reference number, from its FCB; "" when unknown.
// System 7 keeps the FCBs as one table behind FCBSPtr, 94 bytes each, the
// reference number being the offset of the FCB. Mac OS 9 switches to its
// own scheme once booted (FSFCBLen becomes 4, the table holds no names), so
// the name is only read from a classic table, and only when the FCB's volume
// pointer names a mounted volume and the name looks like one (1 to 31
// characters, no control characters).
static std::string file_name(int16 refnum)
{
	if (refnum == 1)
		return "(ROM)";	// the ROM's resource map has reference number 1 and no FCB
	uint32 fcbs = ReadMacInt32(FCBSPtr);
	int16 fcb_length = (int16)ReadMacInt16(FSFCBLen);
	if (refnum <= 0 || fcb_length != CLASSIC_FCB_LENGTH || !valid(fcbs, 2))
		return "";
	uint16 buffer_size = ReadMacInt16(fcbs);
	if ((refnum - 2) % fcb_length != 0 || (uint32)refnum + fcb_length > buffer_size)
		return "";
	if (!known_volume(ReadMacInt32(fcbs + refnum + FCB_VPTR)))
		return "";
	uint32 name = fcbs + refnum + FCB_CNAME;
	uint32 length = ReadMacInt8(name);
	if (length == 0 || length > 31)
		return "";
	for (uint32 i = 1; i <= length; i++) {
		uint8 c = ReadMacInt8(name + i);
		if (c < 0x20 || c == 0x7f)
			return "";
	}
	return pascal_string(name);
}

struct Found {
	int16 refnum;
	uint32 type;
	int16 id;
	std::string name;
	uint8 attributes;
	uint32 data_offset;
};

// Search every open resource map for the reference entry that holds handle.
static bool find_resource(uint32 handle, Found &found)
{
	uint32 map_handle = ReadMacInt32(TopMapHndl);
	for (int maps = 0; maps < 1000 && map_handle != 0; maps++) {
		if (!valid(map_handle, 4))
			return false;
		uint32 map = ReadMacInt32(map_handle);
		if (map == 0 || !valid(map, 28))
			return false;
		uint32 type_list = map + ReadMacInt16(map + MAP_TYPE_LIST);
		uint32 name_list = map + ReadMacInt16(map + MAP_NAME_LIST);
		if (valid(type_list, 2)) {
			int types = (int16)ReadMacInt16(type_list) + 1;	// stored as count - 1
			for (int t = 0; t < types && valid(type_list + 2 + 8 * t, 8); t++) {
				uint32 type_entry = type_list + 2 + 8 * t;
				uint32 type = ReadMacInt32(type_entry);
				int count = (int16)ReadMacInt16(type_entry + 4) + 1;
				uint32 ref_list = type_list + ReadMacInt16(type_entry + 6);
				for (int r = 0; r < count; r++) {
					uint32 entry = ref_list + REF_ENTRY_SIZE * r;
					if (!valid(entry, REF_ENTRY_SIZE))
						break;
					if (ReadMacInt32(entry + 8) != handle)
						continue;
					found.refnum = (int16)ReadMacInt16(map + MAP_REFNUM);
					found.type = type;
					found.id = (int16)ReadMacInt16(entry);
					int16 name_offset = (int16)ReadMacInt16(entry + 2);
					found.name = name_offset == -1 ? "" : pascal_string(name_list + (uint16)name_offset);
					found.attributes = ReadMacInt8(entry + 4);
					found.data_offset = ReadMacInt32(entry + 4) & 0xffffff;
					return true;
				}
			}
		}
		map_handle = ReadMacInt32(map + MAP_NEXT);
	}
	return false;
}

static void initialise(void)
{
	initialised = true;
	const char *path = getenv("MACEMU_TRACE_RSRC");
	if (path == NULL || *path == 0)
		return;
	trace_file = strcmp(path, "-") == 0 ? stderr : fopen(path, "w");
	if (trace_file == NULL) {
		fprintf(stderr, "MACEMU_TRACE_RSRC: cannot write %s\n", path);
		return;
	}
	const char *all = getenv("MACEMU_TRACE_RSRC_ALL");
	trace_all = all != NULL && *all != 0 && strcmp(all, "0") != 0;
	fprintf(trace_file, "ticks\tfile\trefnum\ttype\tid\tname\tattributes\tdata_offset\thint_type\n");
	fflush(trace_file);
}

void RsrcTraceLoad(uint32 hint_type, uint32 handle)
{
	if (!initialised)
		initialise();
	if (trace_file == NULL || handle == 0)
		return;

	Found found;
	bool known = find_resource(handle, found);
	char line[512];
	if (known) {
		std::string file = file_name(found.refnum);
		if (!trace_all) {
			char key[64];
			snprintf(key, sizeof(key), "\t%d\t%08x\t%d", found.refnum, found.type, found.id);
			if (!seen.insert(file + key).second)
				return;
		}
		snprintf(line, sizeof(line), "%u\t%s\t%d\t%s\t%d\t%s\t0x%02x\t%u\t%s\n",
			ReadMacInt32(Ticks), file.c_str(), found.refnum,
			four_char_code(found.type).c_str(), found.id, found.name.c_str(),
			found.attributes, found.data_offset, four_char_code(hint_type).c_str());
	} else {
		// Not in any open map (a detached resource, or a map we cannot read).
		if (!trace_all) {
			char key[64];
			snprintf(key, sizeof(key), "?\t%08x\t%08x", hint_type, handle);
			if (!seen.insert(key).second)
				return;
		}
		snprintf(line, sizeof(line), "%u\t\t\t\t\t\t\t\t%s\n",
			ReadMacInt32(Ticks), four_char_code(hint_type).c_str());
	}
	fputs(line, trace_file);
	fflush(trace_file);
}
