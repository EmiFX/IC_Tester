#ifndef LOGIC_GATE_TEST_H
#define LOGIC_GATE_TEST_H

#include <stdint.h>


// ## REFERENCIAS EXTERNAS ##
extern const int8_t IC_PINS[14];
extern uint8_t      pinState[14];

uint8_t check_overcurrent(void);


// ## MANEJO DE PINES ##
void    pin_set_input(uint8_t pin);
void    pin_set_output(uint8_t pin);
void    pin_write(uint8_t pin, uint8_t val);
uint8_t pin_read(uint8_t pin);
void    release_all_pins(void);


// ## PRUEBAS DE ICs ##
uint8_t test_7404(void);
uint8_t test_7408(void);
uint8_t test_7432(void);


#endif // LOGIC_GATE_TEST_H
