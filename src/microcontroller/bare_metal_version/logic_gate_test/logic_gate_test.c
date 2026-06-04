#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdint.h>
#include "logic_gate_test.h"


// ## MANEJO DE PINES ##
// Mapeo de pines de Arduino a puertos del ATmega328P:
//   D0-D7  -> PORTD bits 0-7
//   D8-D13 -> PORTB bits 0-5
//   A0-A5  -> PORTC bits 0-5 (se pasan como 14-19)

void pin_set_input(uint8_t pin)
{
    if (pin <= 7)
    {
        DDRD  &= ~(1u << pin);
        PORTD &= ~(1u << pin);
    }
    else if (pin <= 13)
    {
        DDRB  &= ~(1u << (pin - 8));
        PORTB &= ~(1u << (pin - 8));
    }
    else
    {
        DDRC  &= ~(1u << (pin - 14));
        PORTC &= ~(1u << (pin - 14));
    }
}

void pin_set_output(uint8_t pin)
{
    if (pin <= 7)       DDRD |= (1u << pin);
    else if (pin <= 13) DDRB |= (1u << (pin - 8));
    else                DDRC |= (1u << (pin - 14));
}

void pin_write(uint8_t pin, uint8_t val)
{
    if (pin <= 7)
    {
        if (val) PORTD |=  (1u << pin);
        else     PORTD &= ~(1u << pin);
    }
    else if (pin <= 13)
    {
        if (val) PORTB |=  (1u << (pin - 8));
        else     PORTB &= ~(1u << (pin - 8));
    }
    else
    {
        if (val) PORTC |=  (1u << (pin - 14));
        else     PORTC &= ~(1u << (pin - 14));
    }
}

uint8_t pin_read(uint8_t pin)
{
    if (pin <= 7)       return (PIND >> pin)        & 1u;
    else if (pin <= 13) return (PINB >> (pin - 8))  & 1u;
    else                return (PINC >> (pin - 14)) & 1u;
}

void release_all_pins(void)
{
    for (uint8_t i = 0; i < 14; i++)
    {
        if (IC_PINS[i] < 0) continue;
        pin_set_input((uint8_t)IC_PINS[i]);
    }
}


// ## TABLAS DE COMPUERTAS ##
// Cada fila = una compuerta del IC, usando indices de IC_PINS[] (base cero)

// 7404 (inversor hex): 6 compuertas, cada una con [entrada, salida]
static const uint8_t GATES_7404[6][2] = {
    { 0,  1},   // pin1  -> pin2
    { 2,  3},   // pin3  -> pin4
    { 4,  5},   // pin5  -> pin6
    {12, 11},   // pin13 -> pin12
    {10,  9},   // pin11 -> pin10
    { 8,  7},   // pin9  -> pin8
};

// 7408 (AND quad) y 7432 (OR quad): 4 compuertas, cada una con [entradaA, entradaB, salida]
static const uint8_t GATES_2IN[4][3] = {
    { 0,  1,  2},   // pin1,  pin2  -> pin3
    { 3,  4,  5},   // pin4,  pin5  -> pin6
    { 9,  8,  7},   // pin10, pin9  -> pin8
    {12, 11, 10},   // pin13, pin12 -> pin11
};


// ## PRUEBA 7404 - INVERSOR (1 entrada, 1 salida) ##
uint8_t test_7404(void)
{
    uint8_t all_pass    = 1;
    uint8_t overcurrent = 0;
    memset(pinState, 0, 14);
    release_all_pins();

    for (uint8_t g = 0; g < 6 && !overcurrent; g++)
    {
        uint8_t in_idx  = GATES_7404[g][0];
        uint8_t out_idx = GATES_7404[g][1];
        uint8_t in_pin  = (uint8_t)IC_PINS[in_idx];
        uint8_t out_pin = (uint8_t)IC_PINS[out_idx];
        uint8_t g_pass  = 1;

        pin_set_input(out_pin);

        for (uint8_t a = 0; a <= 1 && !overcurrent; a++)
        {
            pin_set_output(in_pin);
            pin_write(in_pin, a);
            _delay_us(50);

            if (check_overcurrent()) { overcurrent = 1; all_pass = 0; break; }

            if (pin_read(out_pin) != (uint8_t)(!a)) g_pass = 0;
        }

        if (!overcurrent)
        {
            pinState[in_idx]  = g_pass;
            pinState[out_idx] = g_pass;
            if (!g_pass) all_pass = 0;
        }
    }

    release_all_pins();
    return all_pass;
}


// ## PRUEBA 7408 - AND (2 entradas, 1 salida) ##
uint8_t test_7408(void)
{
    uint8_t all_pass    = 1;
    uint8_t overcurrent = 0;
    memset(pinState, 0, 14);
    release_all_pins();

    for (uint8_t g = 0; g < 4 && !overcurrent; g++)
    {
        uint8_t inA  = GATES_2IN[g][0];
        uint8_t inB  = GATES_2IN[g][1];
        uint8_t out  = GATES_2IN[g][2];
        uint8_t pinA = (uint8_t)IC_PINS[inA];
        uint8_t pinB = (uint8_t)IC_PINS[inB];
        uint8_t pinY = (uint8_t)IC_PINS[out];
        uint8_t g_pass = 1;

        pin_set_input(pinY);

        for (uint8_t ab = 0; ab < 4 && !overcurrent; ab++)
        {
            uint8_t a = (ab >> 1) & 1;
            uint8_t b =  ab       & 1;

            pin_set_output(pinA); pin_write(pinA, a);
            pin_set_output(pinB); pin_write(pinB, b);
            _delay_us(50);

            if (check_overcurrent()) { overcurrent = 1; all_pass = 0; break; }

            if (pin_read(pinY) != (a & b)) g_pass = 0;
        }

        if (!overcurrent)
        {
            pinState[inA] = pinState[inB] = pinState[out] = g_pass;
            if (!g_pass) all_pass = 0;
        }
    }

    release_all_pins();
    return all_pass;
}


// ## PRUEBA 7432 - OR (2 entradas, 1 salida) ##
uint8_t test_7432(void)
{
    uint8_t all_pass    = 1;
    uint8_t overcurrent = 0;
    memset(pinState, 0, 14);
    release_all_pins();

    for (uint8_t g = 0; g < 4 && !overcurrent; g++)
    {
        uint8_t inA  = GATES_2IN[g][0];
        uint8_t inB  = GATES_2IN[g][1];
        uint8_t out  = GATES_2IN[g][2];
        uint8_t pinA = (uint8_t)IC_PINS[inA];
        uint8_t pinB = (uint8_t)IC_PINS[inB];
        uint8_t pinY = (uint8_t)IC_PINS[out];
        uint8_t g_pass = 1;

        pin_set_input(pinY);

        for (uint8_t ab = 0; ab < 4 && !overcurrent; ab++)
        {
            uint8_t a = (ab >> 1) & 1;
            uint8_t b =  ab       & 1;

            pin_set_output(pinA); pin_write(pinA, a);
            pin_set_output(pinB); pin_write(pinB, b);
            _delay_us(50);

            if (check_overcurrent()) { overcurrent = 1; all_pass = 0; break; }

            if (pin_read(pinY) != (a | b)) g_pass = 0;
        }

        if (!overcurrent)
        {
            pinState[inA] = pinState[inB] = pinState[out] = g_pass;
            if (!g_pass) all_pass = 0;
        }
    }

    release_all_pins();
    return all_pass;
}
