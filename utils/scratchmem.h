/*
 * Copyright (c) 2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef SCRATCHMEM_H
#define SCRATCHMEM_H

void ScratchMemInit(void);
void ScratchMemDestroy(void);
unsigned long ScratchMemPos(void);
void *ScratchMemAlloc(unsigned long);
void ScratchMemRewind(unsigned long);


#define SCRATCH_ALLOC(type, size) (static_cast<type>(ScratchMemAlloc(size)))

/* helper to restore scratch position on stack unwind */
class ScratchMemWaypoint
{
public:
    ScratchMemWaypoint() { m_pos = ScratchMemPos(); }
    ~ScratchMemWaypoint() { ScratchMemRewind(m_pos); }

private:
    unsigned long m_pos;
};


#endif // SCRATCHMEM_H
