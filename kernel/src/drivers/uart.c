#include "minemu/platform.h"
#include "minemu/uart.h"
#include "minemu/irq.h"

static volatile char uart_rx_buffer[256];
static volatile uint32_t uart_rx_head = 0;
static volatile uint32_t uart_rx_tail = 0;

void uart_putchar(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
        // Wait for TX buffer to be ready
    }
    MINEMU_UART0->tx_data = (uint32_t)c;
}
void uart_putstring(const char *s) {
    while (*s != '\0') {
        uart_putchar(*s++);
    }
}

// IRQ Handler Function for UART RX Interrupts, drains UART RX into buffer
void uart_rx_irq_handler(void) {
    //Read all characters from UART RX and store in buffer
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        char c = (char)MINEMU_UART0->rx_data;
        // Calculate next head with bitwise & as it is faster than modulo
        uint32_t next_head = (uart_rx_head + 1u) & (sizeof(uart_rx_buffer) - 1u);
        if (next_head != uart_rx_tail) {
            uart_rx_buffer[uart_rx_head] = c;
            uart_rx_head = next_head;
        }
    }
}

// Function for reading characters from UART RX buffer
char uart_getchar(void) {
    for (;;) {
        // Disable interrupts to safely access the buffer
        minemu_irq_disable();
        // Wait for a character to be available in the buffer
        if (uart_rx_head != uart_rx_tail) {
            char c = uart_rx_buffer[uart_rx_tail];
            uart_rx_tail = (uart_rx_tail + 1u) & (sizeof(uart_rx_buffer) - 1u);
            minemu_irq_enable();
            return c;
        }
        minemu_irq_enable();
    }
}

// Function for initializing UART0 RX interrupts
void uart_init(void) {
    // Enable UART RX interrupt
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    // Enable UART0 interrupt in the interrupt controller
    MINEMU_INTERRUPT->enable |= ( UINT32_C(1) << MINEMU_IRQ_UART0);
    // Enable global interrupts
    minemu_irq_enable();
}

