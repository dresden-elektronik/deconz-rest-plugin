/*
 * Copyright (c) 2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "deconz/u_assert.h"
#include "deconz/u_arena.h"
#include "scratchmem.h"

#define INITIAL_SCRATCH_SIZE (1 << 22)  // 4 MB

static U_Arena scratch_arena;

void ScratchMemInit(void)
{
    U_InitArena(&scratch_arena, INITIAL_SCRATCH_SIZE);
}

void ScratchMemDestroy(void)
{
    U_FreeArena(&scratch_arena);
}

unsigned long ScratchMemPos(void)
{
    return scratch_arena.size;
}

void *ScratchMemAlloc(unsigned long size)
{
    U_ASSERT((scratch_arena.size + size + 16) < scratch_arena._total_size);
    if (scratch_arena._total_size < (scratch_arena.size + size + 16))
    {
        // TODO we can't grow since that invalidates pointers
        // if needed old arenas could be preserved until rewind(0) is called
        // but that's rather meh
        return nullptr;
    }

    void *p;
    p = U_AllocArena(&scratch_arena, size, U_ARENA_ALIGN_8);
    return p;
}

void ScratchMemRewind(unsigned long pos)
{
    if (pos < scratch_arena._total_size)
    {
        scratch_arena.size = pos;
    }
}
