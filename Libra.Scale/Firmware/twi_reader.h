#ifndef TWI_READER_H
#define TWI_READER_H

int twi_read_register_data(uint8_t register_pointer);
void twi_write_register_data(uint8_t register_pointer, uint8_t data);
void twi_init(void);

#endif