#include "minemu/platform.h"
#include "minemu/uart.h"

void uart_putchar(char c) {
    if (MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) {
        MINEMU_UART0->tx_data = (uint32_t)c;
    }
}
void uart_putstring(const char *s) {
    while (*s != '\0') {
        uart_putchar(*s++);
    }
}