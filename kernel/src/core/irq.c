#include <stdint.h>
#include "minemu/platform.h"
#include "minemu/irq.h"
#include "minemu/uart.h"

typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[] = {
    [MINEMU_IRQ_UART0] = uart_rx_irq_handler
};

// This function gets called in irq.s when an interrupt occurs and dispatches the correct interrupt handler
struct minemu_trap_frame * minemu_irq_dispatch(struct minemu_trap_frame *frame){
    // Source is the exception ID of the interrupt that was triggered
    uint32_t source = (uint32_t)frame->exception_id;
    // Determine correct IRQ handle function based on source and call it
    if (source == MINEMU_IRQ_NONE) {
        return frame; // No active claim so invalid interrupt, do not signal EOI and just return frame
    } else if (source == MINEMU_IRQ_SYSTICK) { 
        MINEMU_SYSTICK->ack = MINEMU_SYSTICK_ACK;
    } else if (source == MINEMU_IRQ_BLOCK) {
        MINEMU_BLOCK->ack = MINEMU_BLOCK_ACK;
    } else if (source < sizeof(irq_handlers) / sizeof(irq_handlers[0]) && irq_handlers[source] != 0) {
        irq_handlers[source]();
    }
    // Signal end of interrupt to the interrupt controller
    MINEMU_INTERRUPT->eoi = source;
    return frame;
}
