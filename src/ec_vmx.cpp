/*
 * Execution Context
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

#include "dmar.h"
#include "ec.h"
#include "gsi.h"
#include "lapic.h"
#include "vmx.h"
#include "vtlb.h"

void Ec::vmx_exception()
{
    mword intr_info  = Vmcs::read (Vmcs::EXI_INTR_INFO);
    assert (intr_info & 0x80000000);

    // Handle event during IDT vectoring
    mword vect_info  = Vmcs::read (Vmcs::IDT_VECT_INFO);
    mword vect_error = Vmcs::read (Vmcs::IDT_VECT_ERROR);

    if (vect_info & 0x80000000) {
        Vmcs::write (Vmcs::ENT_INTR_INFO,  vect_info);
        Vmcs::write (Vmcs::ENT_INTR_ERROR, vect_error);
        switch (vect_info >> 8 & 0x7) {
            case 4 ... 6:
                Vmcs::write (Vmcs::ENT_INST_LEN, Vmcs::read (Vmcs::EXI_INST_LEN));
                break;
        }
    };

    if (EXPECT_TRUE ((intr_info & 0x7ff) == 0x30e)) {

        mword intr_error = Vmcs::read (Vmcs::EXI_INTR_ERROR);

        switch (Vtlb::miss (&current->regs, Vmcs::read (Vmcs::EXI_QUALIFICATION), intr_error)) {

            case Vtlb::GPA_HPA:
                current->regs.dst_portal = Vmcs::VMX_EPT_VIOLATION;
                break;

            case Vtlb::GLA_GPA:
                Vmcs::write (Vmcs::ENT_INTR_INFO,  intr_info);
                Vmcs::write (Vmcs::ENT_INTR_ERROR, intr_error);

            case Vtlb::SUCCESS:
                ret_user_vmresume();
        }

    } else if (EXPECT_TRUE ((intr_info & 0x7ff) == 0x307)) {

        handle_exc_nm();
        ret_user_vmresume();

    } else
        current->regs.dst_portal = Vmcs::VMX_EXCEPTION;

    send_msg<ret_user_vmresume>();
}

void Ec::vmx_extint()
{
    unsigned vector = Vmcs::read (Vmcs::EXI_INTR_INFO) & 0xff;

    if (vector >= VEC_IPI)
        Lapic::ipi_vector (vector);
    else if (vector >= VEC_MSI)
        Dmar::vector (vector);
    else if (vector >= VEC_LVT)
        Lapic::lvt_vector (vector);
    else if (vector >= VEC_GSI)
        Gsi::vector (vector);

    ret_user_vmresume();
}

void Ec::vmx_invlpg()
{
    current->regs.vtlb->flush_addr (Vmcs::read (Vmcs::EXI_QUALIFICATION), Vmcs::vpid());

    Vmcs::adjust_rip();
    ret_user_vmresume();
}

void Ec::vmx_cr()
{
    mword qual = Vmcs::read (Vmcs::EXI_QUALIFICATION);

    unsigned gpr = qual >> 8 & 0xf;
    unsigned acc = qual >> 4 & 0x3;
    unsigned cr  = qual      & 0xf;

    switch (acc) {
        case 0:     // MOV to CR
            current->regs.write_cr (cr, current->regs.read_gpr (gpr));
            break;
        case 1:     // MOV from CR
            assert (cr != 0 && cr != 4);
            current->regs.write_gpr (gpr, current->regs.read_cr (cr));
            break;
        case 2:     // CLTS
            current->regs.write_cr (cr, current->regs.read_cr (cr) & ~Cpu::CR0_TS);
            break;
        default:
            UNREACHED;
    }

    Vmcs::adjust_rip();
    ret_user_vmresume();
}

void Ec::handle_vmx()
{
    Cpu::hazard |= Cpu::HZD_DS_ES | Cpu::HZD_TR;

    mword reason = Vmcs::read (Vmcs::EXI_REASON) & 0xff;

    Counter::vmi[reason]++;

    switch (reason) {
        case Vmcs::VMX_EXCEPTION:   vmx_exception();
        case Vmcs::VMX_EXTINT:      vmx_extint();
        case Vmcs::VMX_INVLPG:      vmx_invlpg();
        case Vmcs::VMX_CR:          vmx_cr();
    }

    current->regs.dst_portal = reason;

    send_msg<ret_user_vmresume>();
}
