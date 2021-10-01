#ifndef SPI_READER_H
#define SPI_READER_H

uint32_t spi_read_data(void);
void spi_init(uint32_t param_gain_rate);
void spi_power_off(void);
void spi_set_gain(uint32_t param_gain_rate);

#endif