/*
 * Virtual Translation Lookaside Buffer (VTLB)
 *
 * Copyright (C) 2009-2010 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "compiler.h"
#include "paging.h"

class Exc_regs;

class Vtlb : public Paging
{
    private:
        ALWAYS_INLINE
        static inline size_t walk (Exc_regs *, mword, mword &, mword &, mword &);

        void flush_ptab (unsigned);

    public:
        enum Reason
        {
            SUCCESS,
            GLA_GPA,
            GPA_HPA
        };

        ALWAYS_INLINE
        static inline void *operator new (size_t)
        {
            return Buddy::allocator.alloc (0, Buddy::NOFILL);
        }

        ALWAYS_INLINE
        static inline void operator delete (void *ptr)
        {
            Buddy::allocator.free (reinterpret_cast<mword>(ptr));
        }

        ALWAYS_INLINE
        inline Vtlb()
        {
            for (unsigned i = 0; i < 1ul << bpl; i++)
                this[i].val = ATTR_SUPERPAGE;
        }

        void flush_addr (mword, unsigned long);
        void flush (unsigned, unsigned long);

        static Reason miss (Exc_regs *, mword, mword &);
        static bool load_pdpte (Exc_regs *, uint64 (&)[4]);
};
