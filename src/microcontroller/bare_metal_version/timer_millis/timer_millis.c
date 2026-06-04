#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "timer_millis.h"


// ## VARIABLES DEL TEMPORIZADOR ##
// volatile: el compilador no puede cachear estas variables porque la ISR las modifica en cualquier momento
volatile uint32_t timer0_millis = 0;
static   uint8_t  timer0_fract  = 0;


// ## ISR DE DESBORDAMIENTO DE TIMER0 ##
// Se ejecuta ~976 veces/segundo (cada vez que Timer0 llega a 255 y vuelve a 0).
// Cada desbordamiento dura 1.024ms, no 1ms exacto.
// El error acumulado de 0.024ms se corrige sumando 1ms extra cada 125 desbordes (= 3ms acumulado).
ISR(TIMER0_OVF_vect)
{
    timer0_millis++;
    timer0_fract += 3;
    if (timer0_fract >= 125)
    {
        timer0_fract -= 125;
        timer0_millis++;
    }
}


// ## millis() ##
// Devuelve milisegundos desde que se inicializo Timer0.
// El AVR es de 8 bits: leer un dato de 32 bits toma 4 instrucciones.
// Se deshabilitan interrupciones momentaneamente para evitar leer un valor parcialmente actualizado.
uint32_t millis(void)
{
    uint32_t m;
    uint8_t sreg = SREG;   // guardar estado de interrupciones
    cli();                  // deshabilitar interrupciones
    m = timer0_millis;
    SREG = sreg;            // restaurar interrupciones
    return m;
}


// ## INICIALIZACION DE TIMER0 ##
void timer0_init(void)
{
    // Modo normal (contador 0-255 sin PWM)
    TCCR0A = 0x00;

    // Prescaler /64: 16MHz / 64 / 256 = 976.5 Hz -> ~1.024ms por desbordamiento
    TCCR0B = (1<<CS01)|(1<<CS00);

    // Habilitar interrupcion de desbordamiento -> activa ISR(TIMER0_OVF_vect)
    TIMSK0 = (1<<TOIE0);
}
