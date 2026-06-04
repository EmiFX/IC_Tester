#ifndef INA219_H
#define INA219_H

#include <stdint.h>


// ## CONFIGURACION DEL SENSOR INA219 ##
#define INA219_ADDR     0x40   // direccion I2C del INA219 (pines A0=A1=A2 a GND)
#define INA219_REG_CFG  0x00   // registro de configuracion del sensor
#define INA219_REG_CAL  0x05   // registro de calibracion (determina el LSB de corriente)
#define INA219_REG_CURR 0x04   // registro donde se lee la corriente medida
#define INA219_CAL_VAL  4096   // calibracion: hace que cada bit del registro = 0.1mA


// ## PROTOCOLO TWI (I2C) ##
void    twi_init(void);
void    twi_start(void);
void    twi_stop(void);
void    twi_write_byte(uint8_t data);
uint8_t twi_read_ack(void);
uint8_t twi_read_nack(void);


// ## INA219 ##
void    ina219_write_reg(uint8_t reg, uint16_t val);
int16_t ina219_read_reg(uint8_t reg);
void    ina219_init(void);
float   ina219_get_mA(void);


#endif // INA219_H
