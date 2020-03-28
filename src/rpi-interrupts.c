#include <plibc/stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/rpi-base.h>
#include <kernel/rpi-armtimer.h>
#include <kernel/rpi-interrupts.h>
#include <device/uart0.h>

#define INTERRUPTS_PENDING (RPI_INTERRUPT_CONTROLLER_BASE + 0x200)

#define IRQ_IS_BASIC(x) ((x >= 64))
#define IRQ_IS_GPU2(x) ((x >= 32 && x < 64))
#define IRQ_IS_GPU1(x) ((x < 32))
#define IRQ_IS_PENDING(regs, num) ((IRQ_IS_BASIC(num) && ((1 << (num - 64)) & regs->IRQ_basic_pending)) || (IRQ_IS_GPU2(num) && ((1 << (num - 32)) & regs->IRQ_pending_2)) || \
                                   (IRQ_IS_GPU1(num) && ((1 << (num)) & regs->IRQ_pending_1)))

#define NUM_IRQS 72


/** @brief The BCM2835 Interupt controller peripheral at it's base address */
static rpi_irq_controller_t *rpiIRQController =
    (rpi_irq_controller_t *)RPI_INTERRUPT_CONTROLLER_BASE;

static interrupt_handler_f handlers[NUM_IRQS];
static interrupt_clearer_f clearers[NUM_IRQS];

volatile int32_t count_irqs = 0;

void bzero(void *s, size_t n);

/**
    @brief Return the IRQ Controller register set
*/
rpi_irq_controller_t *RPI_GetIrqController(void)
{
    return rpiIRQController;
}

/**
    @brief The Reset vector interrupt handler

    This can never be called, since an ARM core reset would also reset the
    GPU and therefore cause the GPU to start running code again until
    the ARM is handed control at the end of boot loading
*/
void __attribute__((interrupt("ABORT"))) reset_vector(void)
{
    uart_puts("ABORT INTERRUPT OCCURRED");
    while (1)
        ;
}

/**
    @brief The undefined instruction interrupt handler

    If an undefined intstruction is encountered, the CPU will start
    executing this function. Just trap here as a debug solution.
*/
void __attribute__((interrupt("UNDEF"))) undefined_instruction_vector(void)
{
    uart_puts("Undef Interrupt Occurred");
    while (1)
        ;
}

/**
    @brief The supervisor call interrupt handler

    The CPU will start executing this function. Just trap here as a debug
    solution.
*/
void __attribute__((interrupt("SWI"))) software_interrupt_vector(void)
{
    uart_puts("Software Interrupt Occurred");
}

void __attribute__((interrupt("ABORT"))) prefetch_abort_vector(void)
{
    uart_puts("prefetch_abort_vector Interrupt Occurred");
    while (1)
        ;
}

void __attribute__((interrupt("ABORT"))) data_abort_vector(void)
{
    uart_puts("data_abort_vector Interrupt Occurred");
    while (1)
        ;
}

void __attribute__((interrupt("FIQ"))) fast_interrupt_vector(void)
{
    uart_puts("fast_interrupt_vector Interrupt Occurred");
}

void interrupts_init(void)
{
    // _enable_interrupts();
    DISABLE_INTERRUPTS();
    bzero(handlers, sizeof(interrupt_handler_f) * NUM_IRQS);
    bzero(clearers, sizeof(interrupt_clearer_f) * NUM_IRQS);
    rpiIRQController->Disable_Basic_IRQs = 0xffffffff; // disable all interrupts
    rpiIRQController->Disable_IRQs_1 = 0xffffffff;
    rpiIRQController->Disable_IRQs_2 = 0xffffffff;
    __asm__ __volatile__("cpsie i");
    //  _enable_interrupts();
    ENABLE_INTERRUPTS();
}

/**
 * this function is going to be called by the processor.  Needs to check pending interrupts and execute handlers if one is registered
 */
void irq_handler(void)
{
    // printf("IRQ triggered");
    int32_t j;
    for (j = 0; j < NUM_IRQS; j++)
    {
        // If the interrupt is pending and there is a handler, run the handler
        if (IRQ_IS_PENDING(rpiIRQController, j) && (handlers[j] != 0))
        {
            clearers[j]();
            ENABLE_INTERRUPTS();
            // printf("Calling handler");
            handlers[j]();
            DISABLE_INTERRUPTS();
            return;
        }
    }
}

void register_irq_handler(irq_number_t irq_num, interrupt_handler_f handler, interrupt_clearer_f clearer)
{
    uint32_t irq_pos;
    if (IRQ_IS_BASIC(irq_num))
    {
        irq_pos = irq_num - 64;
        handlers[irq_num] = handler;
        clearers[irq_num] = clearer;
        rpiIRQController->Enable_Basic_IRQs |= (1 << irq_pos);
    }
    else if (IRQ_IS_GPU2(irq_num))
    {
        // printf("registering GPU1 IRQ2 \n");
        irq_pos = irq_num - 32;
        handlers[irq_num] = handler;
        clearers[irq_num] = clearer;
        // printf(" IRQ2: %x \n ", rpiIRQController->Enable_IRQs_2);
        rpiIRQController->Enable_IRQs_2 |= (1 << ((irq_num) & (32 - 1)));
        // printf("After IRQ2: %x \n ", rpiIRQController->Enable_IRQs_2);
    }
    else if (IRQ_IS_GPU1(irq_num))
    {
        // printf("registering GPU1 IRQ1 \n");
        irq_pos = irq_num;
        handlers[irq_num] = handler;
        clearers[irq_num] = clearer;
        rpiIRQController->Enable_IRQs_1 |= (1 << irq_pos);
    }
    else
    {
        printf("ERROR: CANNOT REGISTER IRQ HANDLER: INVALID IRQ NUMBER: %d\n", irq_num);
    }
}

void unregister_irq_handler(irq_number_t irq_num)
{
    uint32_t irq_pos;
    if (IRQ_IS_BASIC(irq_num))
    {
        irq_pos = irq_num - 64;
        handlers[irq_num] = 0;
        clearers[irq_num] = 0;
        // Setting the disable bit clears the enabled bit
        rpiIRQController->Disable_Basic_IRQs |= (1 << irq_pos);
    }
    else if (IRQ_IS_GPU2(irq_num))
    {
        irq_pos = irq_num - 32;
        handlers[irq_num] = 0;
        clearers[irq_num] = 0;
        rpiIRQController->Disable_IRQs_2 |= (1 << irq_pos);
    }
    else if (IRQ_IS_GPU1(irq_num))
    {
        irq_pos = irq_num;
        handlers[irq_num] = 0;
        clearers[irq_num] = 0;
        rpiIRQController->Disable_IRQs_1 |= (1 << irq_pos);
    }
    else
    {
        printf("ERROR: CANNOT UNREGISTER IRQ HANDLER: INVALID IRQ NUMBER: %d\n", irq_num);
    }
}

void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}
