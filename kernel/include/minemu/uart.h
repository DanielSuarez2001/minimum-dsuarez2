#ifndef UART_H
#define UART_H

void uart_putchar(char c);
void uart_putstring(const char *s);

void uart_rx_irq_handler(void) ;
char uart_getchar(void);

void uart_init(void);

#endif // UART_H