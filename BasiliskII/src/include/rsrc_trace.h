/*
 *  rsrc_trace.h - log the resources the Mac loads (MACEMU_TRACE_RSRC)
 *
 *  Basilisk II (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef RSRC_TRACE_H
#define RSRC_TRACE_H

// Called wherever the emulator sees the Resource Manager hand out a resource
// (the vCheckLoad patch, and SheepShaver's PowerPC GetResource patch).
// hint_type is the type the patch believes it is (not always right: the 68k
// vCheckLoad patch takes it from D3, which preloading can leave stale);
// handle is the resource's handle. Does nothing unless MACEMU_TRACE_RSRC is set.
extern void RsrcTraceLoad(uint32 hint_type, uint32 handle);

#endif
