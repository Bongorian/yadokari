//
// main.c
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include "../src/alloc_policy.h"
#include <circle/startup.h>

// Reject a Circle profile that silently leaks freed project/codec allocations.
static constexpr unsigned kHeapBuckets[]={HEAP_BLOCK_BUCKET_SIZES};
static_assert(sizeof(kHeapBuckets)/sizeof(*kHeapBuckets)<=20,"Circle supports at most 20 heap buckets");
static_assert(kHeapBuckets[sizeof(kHeapBuckets)/sizeof(*kHeapBuckets)-1]>=sizeof(groove::ProjectState),"Use the repository Circle profile: ProjectState must fit a reusable heap bucket");

int main (void)
{
	// cannot return here because some destructors used in CKernel are not implemented

	// Circle initializes its heap before main. Keep the 46 KiB resident
	// kernel off the 128 KiB main stack, which also hosts UI/file calls.
	CKernel* Kernel = groove::alloc_new<CKernel>();
	if (!Kernel || !Kernel->Initialize ())
	{
		halt ();
		return EXIT_HALT;
	}
	
	TShutdownMode ShutdownMode = Kernel->Run ();

	switch (ShutdownMode)
	{
	case ShutdownReboot:
		reboot ();
		return EXIT_REBOOT;

	case ShutdownHalt:
	default:
		halt ();
		return EXIT_HALT;
	}
}
