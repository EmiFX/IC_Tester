#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdint.h>

#include "timer_millis/timer_millis.h"      // Timer0, millis()
#include "uart/uart.h"                      // USART 115200 baud
#include "ina219/ina219.h"                  // TWI + sensor de corriente INA219
#include "logic_gate_test/logic_gate_test.h"// pruebas de compuertas 7404, 7408, 7432


// ## MAPEO DE PINES DEL IC ##
// Indice i = pin fisico del IC (i+1), base cero. -1 = pin de alimentacion (VCC/GND).
const int8_t IC_PINS[14] = {
     8,   // IC pin  1 -> D8  (PB0)
     9,   // IC pin  2 -> D9  (PB1)
    10,   // IC pin  3 -> D10 (PB2)
    11,   // IC pin  4 -> D11 (PB3)
    12,   // IC pin  5 -> D12 (PB4)
    13,   // IC pin  6 -> D13 (PB5)
    -1,   // IC pin  7     GND
     7,   // IC pin  8 -> D7  (PD7)
     6,   // IC pin  9 -> D6  (PD6)
     5,   // IC pin 10 -> D5  (PD5)
     4,   // IC pin 11 -> D4  (PD4)
     3,   // IC pin 12 -> D3  (PD3)
     2,   // IC pin 13 -> D2  (PD2)
    -1,   // IC pin 14     VCC
};


// ## CONFIGURACION ##
// El relay esta en A0 = PC0, modulo activo-LOW (LOW = energizado)
#define RELAY_BIT       PC0
#define OVERCURRENT_MA  100.0f   // abrir relay si la corriente supera este valor (mA)
#define BLANKING_MS        50UL  // ignorar lecturas de corriente justo despues de cerrar el relay


// ## VARIABLES GLOBALES ##
uint8_t  pinState[14]     = {};  // resultado de prueba por pin del IC: 1=paso, 0=fallo/alimentacion
uint16_t selected_ic      = 0;   // IC seleccionado (7404, 7408, 7432)
uint8_t  relay_open       = 1;   // 1 = relay abierto (IC sin alimentacion), 0 = cerrado
uint32_t relay_close_time = 0;   // millis() cuando se cerro el relay por ultima vez


// ## CONTROL DEL RELAY ##

void set_relay(uint8_t open)
{
    if (open)
    {
        // HIGH = desenergizan la bobina del relay (modulo activo-LOW), el contacto se abre
        PORTC |= (1<<RELAY_BIT);
    }
    else
    {
        // LOW = energiza la bobina del relay, el contacto se cierra -> el IC recibe alimentacion
        PORTC &= ~(1<<RELAY_BIT);
        relay_close_time = millis();
    }
    relay_open = open;
}

uint8_t in_blanking_window(void)
{
    // Justo despues de cerrar el relay hay corriente de arranque transitoria
    // Se ignoran las lecturas durante BLANKING_MS milisegundos para evitar falsos disparos
    return (!relay_open) && (millis() - relay_close_time < BLANKING_MS);
}


// ## PROTECCION POR SOBRECORRIENTE ##
// Llamada desde las funciones de prueba (logic_gate_test.h) y desde el loop principal

uint8_t check_overcurrent(void)
{
    if (in_blanking_window()) return 0;

    float mA = ina219_get_mA();

    if (mA > OVERCURRENT_MA)
    {
        set_relay(1);
        uart_puts("OVERCURRENT\n");
        return 1;
    }
    return 0;
}


// ## FUNCIONES DE SALIDA SERIAL ##

void send_pins(void)
{
    uart_puts("PINS:");
    for (uint8_t i = 0; i < 14; i++)
        uart_putc((IC_PINS[i] < 0) ? '0' : ('0' + pinState[i]));
    uart_putc('\n');
}

void send_current(void)
{
    uart_puts("I:");
    if (in_blanking_window())
    {
        uart_puts("0.00\n");
        return;
    }
    uart_put_mA(ina219_get_mA());
    uart_putc('\n');
}


// ## PROCESAMIENTO DE COMANDOS ##

void process_command(const char *cmd)
{
    if      (strcmp(cmd, "7404") == 0) { selected_ic = 7404; }
    else if (strcmp(cmd, "7408") == 0) { selected_ic = 7408; }
    else if (strcmp(cmd, "7432") == 0) { selected_ic = 7432; }
    else if (strcmp(cmd, "START") == 0)
    {
        if (selected_ic == 0) return;

        if (relay_open)
            set_relay(0);
        _delay_ms(100);   // siempre esperar: relay recien cerrado necesita estabilizarse, o pines del test anterior necesitan asentarse

        uint8_t result = 0;
        if      (selected_ic == 7404) result = test_7404();
        else if (selected_ic == 7408) result = test_7408();
        else if (selected_ic == 7432) result = test_7432();

        if (!relay_open)
        {
            send_pins();
            uart_puts(result ? "RESULT:PASS\n" : "RESULT:FAIL\n");
        }
    }
}


// ## MAIN ##

int main(void)
{
    // DDRC: Data Direction Register del Puerto C
    // Poner el bit PC0 en 1 hace que A0 sea pin de salida (para controlar el relay)
    DDRC |= (1<<RELAY_BIT);

    set_relay(1);       // iniciar con relay abierto: el IC no recibe alimentacion todavia
    release_all_pins(); // todos los pines de datos del IC como entradas (alta impedancia)

    timer0_init();
    uart_init();
    ina219_init();

    // sei(): activa el flag global de habilitacion de interrupciones en SREG
    // Sin esto, ninguna ISR se ejecutara (la ISR de desbordamiento de Timer0 lo necesita)
    sei();

    uart_puts("READY\n");

    char     cmd_buf[8];
    uint8_t  cmd_len      = 0;
    uint32_t last_curr_ms = 0;

    while (1)
    {
        // Enviar lectura de corriente cada 250ms
        uint32_t now = millis();
        if (now - last_curr_ms >= 250UL)
        {
            last_curr_ms = now;
            send_current();
        }

        // Verificar sobrecorriente mientras el relay este cerrado
        if (!relay_open) check_overcurrent();

        // Leer un byte por vez del UART (no bloqueante).
        // uart_available() solo pregunta si llego algo — no espera, no bloquea el loop.
        if (uart_available())
        {
            char c = uart_getc();

            if (c == '\n' || c == '\r')
            {
                // Algunos terminales envian "\r\n" (Windows); descartar el '\r' sobrante
                if (cmd_len > 0 && cmd_buf[cmd_len - 1] == '\r') cmd_len--;

                cmd_buf[cmd_len] = '\0';

                if (cmd_len > 0) process_command(cmd_buf);

                cmd_len = 0;
            }
            else if (cmd_len < 7)
            {
                // Guardar hasta 7 caracteres; posicion 7 reservada para '\0'
                // Ignorar bytes extra para evitar desbordamiento del buffer
                cmd_buf[cmd_len++] = c;
            }
        }
    }
}
