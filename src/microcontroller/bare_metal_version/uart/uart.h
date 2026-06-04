#ifndef UART_H
#define UART_H

#include <stdint.h>

void    uart_init(void);
void    uart_putc(char c);
void    uart_puts(const char *s);
uint8_t uart_available(void);
char    uart_getc(void);
void    uart_put_mA(float mA);

#endif // UART_H
