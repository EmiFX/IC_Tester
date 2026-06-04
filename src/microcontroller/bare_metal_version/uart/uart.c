#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <stdint.h>
#include "uart.h"


// ## INICIALIZACION USART A 115200 BAUD ##
void uart_init(void)
{
    // Formula con U2X0=1 (modo doble velocidad): UBRR = F_CPU / (8 * BAUD) - 1 = 16
    UBRR0H = 0;
    UBRR0L = 16;

    // U2X0: reduce error de baud rate de ~8.5% a ~2.1% en 115200
    UCSR0A = (1<<U2X0);

    // Habilitar receptor (RXEN0) y transmisor (TXEN0)
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);

    // 8N1: 8 bits de datos, sin paridad, 1 bit de parada
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}


// ## TRANSMISION ##

void uart_putc(char c)
{
    // Esperar hasta que el buffer de transmision este listo (UDRE0=1)
    while (!(UCSR0A & (1<<UDRE0)));
    UDR0 = (uint8_t)c;
}

void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}


// ## RECEPCION ##

uint8_t uart_available(void)
{
    // RXC0=1 cuando hay un byte listo para leer en UDR0
    return (UCSR0A & (1<<RXC0)) ? 1 : 0;
}

char uart_getc(void)
{
    return (char)UDR0;
}


// ## ENVIO DE NUMERO FLOTANTE ##
// Convierte miliamperes a texto "NNN.NN" sin usar printf (ahorra ~1.5KB de flash).
void uart_put_mA(float mA)
{
    if (mA < 0.0f) { uart_putc('-'); mA = -mA; }

    uint16_t whole = (uint16_t)mA;
    uint8_t  frac  = (uint8_t)((mA - (float)whole) * 100.0f + 0.5f);
    if (frac >= 100) { whole++; frac -= 100; }

    // Construir parte entera (digitos salen invertidos, luego se invierten)
    char   tmp[6];
    int8_t idx = 0;
    if (whole == 0)
    {
        tmp[idx++] = '0';
    }
    else
    {
        uint16_t n = whole;
        while (n) { tmp[idx++] = (char)('0' + n % 10); n /= 10; }
        for (int8_t l = 0, r = idx - 1; l < r; l++, r--)
        {
            char t = tmp[l]; tmp[l] = tmp[r]; tmp[r] = t;
        }
    }
    tmp[idx] = '\0';
    uart_puts(tmp);

    uart_putc('.');
    uart_putc((char)('0' + frac / 10));
    uart_putc((char)('0' + frac % 10));
}
