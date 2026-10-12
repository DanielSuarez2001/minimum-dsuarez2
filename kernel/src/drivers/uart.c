#include <stddef.h>

#include "minemu/platform.h"
#include "minemu/uart.h"
#include "minemu/irq.h"

static volatile char uart0_rx_buffer[256];
static volatile uint32_t uart0_rx_head = 0;
static volatile uint32_t uart0_rx_tail = 0;

static volatile char uart1_rx_buffer[256];
static volatile uint32_t uart1_rx_head = 0;
static volatile uint32_t uart1_rx_tail = 0;

//For UART0 functions
// IRQ Handler Function for UART RX Interrupts, drains UART RX into buffer
void uart0_rx_irq_handler(void) {
    //Read all characters from UART RX and store in buffer
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        char c = (char)MINEMU_UART0->rx_data;
        // Calculate next head with bitwise & as it is faster than modulo
        uint32_t next_head = (uart0_rx_head + 1u) & (sizeof(uart0_rx_buffer) - 1u);
        if (next_head != uart0_rx_tail) {
            uart0_rx_buffer[uart0_rx_head] = c;
            uart0_rx_head = next_head;
        }
    }
}


void uart0_putchar(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
        // Wait for TX buffer to be ready
    }
    MINEMU_UART0->tx_data = (uint32_t)c;
}
void uart0_putstring(const char *s) {
    while (*s != '\0') {
        uart0_putchar(*s++);
    }
}
size_t uart0_var_putstring(const char *s, size_t max_len) {
    size_t count = 0;
    while (count < max_len) {
        uart0_putchar(*s++);   
        count++;
    }
    return count;
}

// Function for reading characters from UART0 RX buffer
char uart0_getchar(void) {
    for (;;) {
        // Disable interrupts to safely access the buffer
        minemu_irq_disable();
        // Wait for a character to be available in the buffer
        if (uart0_rx_head != uart0_rx_tail) {
            char c = uart0_rx_buffer[uart0_rx_tail];
            uart0_rx_tail = (uart0_rx_tail + 1u) & (sizeof(uart0_rx_buffer) - 1u);
            minemu_irq_enable();
            return c;
        }
        minemu_irq_enable();
    }
}
// Function for draining from UART0 RX buffer into a provided destination buffer
size_t uart0_var_getchars(char *dst, size_t max_len) {
    uart0_rx_irq_handler();
    size_t count = 0;
    while (count < max_len && uart0_rx_head != uart0_rx_tail) {
        // Wait for a character to be available in the buffer
        char c = uart0_rx_buffer[uart0_rx_tail];
        uart0_rx_tail = (uart0_rx_tail + 1u) & (sizeof(uart0_rx_buffer) - 1u);
        dst[count++] = c;
    }
    return count;
}


//For UART1 functions
// IRQ Handler Function for UART1 RX Interrupts, drains UART RX into buffer
void uart1_rx_irq_handler(void) {
    //Read all characters from UART RX and store in buffer
    while (MINEMU_UART1->status & MINEMU_UART_STATUS_RX_READY) {
        char c = (char)MINEMU_UART1->rx_data;
        // Calculate next head with bitwise & as it is faster than modulo
        uint32_t next_head = (uart1_rx_head + 1u) & (sizeof(uart1_rx_buffer) - 1u);
        if (next_head != uart1_rx_tail) {
            uart1_rx_buffer[uart1_rx_head] = c;
            uart1_rx_head = next_head;
        }
    }
}

void uart1_putchar(char c) {
    while (!(MINEMU_UART1->status & MINEMU_UART_STATUS_TX_READY)) {
        // Wait for TX buffer to be ready
    }
    MINEMU_UART1->tx_data = (uint32_t)c;
}
void uart1_putstring(const char *s) {
    while (*s != '\0') {
        uart1_putchar(*s++);
    }
}
size_t uart1_var_putstring(const char *s, size_t max_len) {
    size_t count = 0;
    while (count < max_len) {
        uart1_putchar(*s++);   
        count++;
    }
    return count;
}

// Function for reading characters from UART1 RX buffer
char uart1_getchar(void) {
    for (;;) {
        // Disable interrupts to safely access the buffer
        minemu_irq_disable();
        // Wait for a character to be available in the buffer
        if (uart1_rx_head != uart1_rx_tail) {
            char c = uart1_rx_buffer[uart1_rx_tail];
            uart1_rx_tail = (uart1_rx_tail + 1u) & (sizeof(uart1_rx_buffer) - 1u);
            minemu_irq_enable();
            return c;
        }
        minemu_irq_enable();
    }
}
// Function for draining from UART1 RX buffer into a provided destination buffer
size_t uart1_var_getchars(char *dst, size_t max_len) {
    uart1_rx_irq_handler();
    size_t count = 0;
    while (count < max_len && uart1_rx_head != uart1_rx_tail) {
        // Wait for a character to be available in the buffer
        char c = uart1_rx_buffer[uart1_rx_tail];
        uart1_rx_tail = (uart1_rx_tail + 1u) & (sizeof(uart1_rx_buffer) - 1u);
        dst[count++] = c;
    }
    return count;
}


// Function for initializing UART0 & UART1 RX interrupts
void uart_init(void) {
    // Enable UART RX interrupt
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_UART1->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    // Enable UART0 and UART1 interrupts in the interrupt controller
    MINEMU_INTERRUPT->enable |= ( UINT32_C(1) << MINEMU_IRQ_UART0);
    MINEMU_INTERRUPT->enable |= ( UINT32_C(1) << MINEMU_IRQ_UART1);
    // Enable global interrupts
    minemu_irq_enable();
}

