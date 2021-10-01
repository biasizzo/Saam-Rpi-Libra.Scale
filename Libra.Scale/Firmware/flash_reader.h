#ifndef FLASH_READER_H
#define FLASH_READER_H

void fstorage_init( uint32_t * param_refresh_time,
                    uint32_t * param_refresh_indicate,
                    uint32_t * param_timeout,
                    uint32_t * param_correction_factor,
                    uint32_t * param_gain_rate,
                    char  * param_password,
                    char  * param_name );

void fstorage_uninit();

void fstorage_save( uint32_t * param_refresh_time,
                    uint32_t * param_refresh_indicate,
                    uint32_t * param_timeout,
                    uint32_t * param_correction_factor,
                    uint32_t * param_gain_rate,
                    char  * param_password,
                    char  * param_name );



#endif