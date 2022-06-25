.globl	task_switch
task_switch:					/* r0: pOldRegs, r1: pNewRegs */
	stmia	r0!, {r0, r4-r14} // r0 contains base address of old task regs. Store all the current reg values at the address pointed by RO
	fmrx	r2, fpexc // The FMRX instruction transfers the contents of VFPsysreg into Rd.
	fmrx	r3, fpscr
	stmia 	r0!, {r2-r3}

	fstmiad	r0, {d0-d15} //Floating-point load multiple and store multiple.

	ldmia	r1!, {r0, r4-r14} // r1 contains base address of new task regs. Restore all the reg values from base addres (in R1) into Regs.
	fmxr	fpexc, r2 //The FMXR instruction transfers the contents of Rd into VFPsysreg.
	fmxr	fpscr, r3
	ldmia 	r1!, {r2-r3}
	fldmiad	r1, {d0-d15} //Floating-point load multiple and store multiple.
	bx lr


@ .globl	ret_from_fork
@ ret_from_fork: