#ifndef TIMER_MILLIS_H
#define TIMER_MILLIS_H

#include <stdint.h>


extern volatile uint32_t timer0_millis;

uint32_t millis(void);
void     timer0_init(void);

#endif // TIMER_MILLIS_H
