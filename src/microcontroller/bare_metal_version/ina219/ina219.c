#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <stdint.h>
#include "ina219.h"


// ## PROTOCOLO TWI (I2C) ##

void twi_init(void)
{
    // Prescaler = 1 (TWSR bits 1:0 = 00)
    TWSR = 0x00;

    // TWBR: SCL = F_CPU / (16 + 2 * TWBR * prescaler) = 16MHz / 160 = 100kHz
    TWBR = 72;

    // TWEN: activa el modulo I2C del hardware del ATmega328P
    TWCR = (1<<TWEN);
}

void twi_start(void)
{
    // TWINT se limpia escribiendo 1; TWSTA genera el pulso START
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

void twi_stop(void)
{
    // TWSTO genera el pulso STOP y libera el bus
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
}

void twi_write_byte(uint8_t data)
{
    TWDR = data;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

uint8_t twi_read_ack(void)
{
    // TWEA=1: enviar ACK al esclavo (hay mas bytes por venir)
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

uint8_t twi_read_nack(void)
{
    // Sin TWEA: enviar NACK (este es el ultimo byte)
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}


// ## INA219 ##

void ina219_write_reg(uint8_t reg, uint16_t val)
{
    twi_start();
    twi_write_byte(INA219_ADDR << 1);      // direccion + R/W=0 (escritura)
    twi_write_byte(reg);
    twi_write_byte((uint8_t)(val >> 8));   // byte alto
    twi_write_byte((uint8_t)(val));        // byte bajo
    twi_stop();
}

int16_t ina219_read_reg(uint8_t reg)
{
    // Fase escritura: apuntar al registro
    twi_start();
    twi_write_byte(INA219_ADDR << 1);
    twi_write_byte(reg);

    // Repeated START para cambiar a lectura sin soltar el bus
    twi_start();
    twi_write_byte((INA219_ADDR << 1) | 1);  // direccion + R/W=1 (lectura)
    uint8_t hi = twi_read_ack();
    uint8_t lo = twi_read_nack();
    twi_stop();

    return (int16_t)((uint16_t)hi << 8 | lo);
}

void ina219_init(void)
{
    twi_init();
    // Bus 32V, shunt +/-320mV, resolucion 12-bit, modo continuo
    ina219_write_reg(INA219_REG_CFG, 0x399F);
    // Calibracion: 4096 -> cada bit del registro de corriente = 0.1mA
    ina219_write_reg(INA219_REG_CAL, INA219_CAL_VAL);
}

float ina219_get_mA(void)
{
    return (float)ina219_read_reg(INA219_REG_CURR) * 0.1f;
}
