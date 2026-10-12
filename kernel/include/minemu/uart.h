#ifndef UART_H
#define UART_H

#include <stddef.h>

//For UART0 functions
void uart0_putchar(char c);
void uart0_putstring(const char *s);
size_t uart0_var_putstring(const char *s, size_t max_len);

void uart0_rx_irq_handler(void) ;
char uart0_getchar(void);
size_t uart0_var_getchars(char *dst, size_t max_len);

//For UART1 functions
void uart1_putchar(char c);
void uart1_putstring(const char *s);
size_t uart1_var_putstring(const char *s, size_t max_len);

void uart1_rx_irq_handler(void) ;
char uart1_getchar(void);
size_t uart1_var_getchars(char *dst, size_t max_len);

//Function for initializing UART0 and UART1
void uart_init(void);

#endif // UART_H