/**
 *
 *   This file contains functions for bit banging HX711
 *
 */

#include "nrf_drv_spi.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "boards.h"
#include "app_error.h"
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define SPI_INSTANCE  2 /**< SPI instance index. */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static volatile bool spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */

          

                                                                    // Gain: 128 => 0x80; 64 => 0xA8; 32 => 0xA0 
static uint8_t       m_tx_buf_pwr_off[] =  {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xA0};
static uint8_t       m_tx_buf[] =    {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xA8, 0x00};
static uint8_t       m_rx_buf[6];    /**< RX buffer. */           
static const uint8_t m_rx_length = sizeof(m_rx_buf);        /**< RX length. */            
static const uint8_t m_tx_length = sizeof(m_tx_buf);        /**< TX length. */
static const uint8_t m_tx_buf_pwr_off_length = sizeof(m_tx_buf_pwr_off);        /**< TX power off length. */
static int32_t      processed_data;


/**
 * @brief Converts 6 byte array to 24bit variable by skipping every 2nd bit.
 * @param 6 byte array
 */
int32_t skip_bits(uint8_t * array)
{
    int32_t value = 0 ;

    value = value | (array[0] & 0b01000000) << (1+16);
    value = value | (array[0] & 0b00010000) << (2+16);
    value = value | (array[0] & 0b00000100) << (3+16);
    value = value | (array[0] & 0b00000001) << (4+16);

    value = value | ((array[1] & 0b01000000) >>3 )<< 16;
    value = value | ((array[1] & 0b00010000) >>2 )<< 16;
    value = value | ((array[1] & 0b00000100) >>1 )<< 16;
    value = value |  (array[1] & 0b00000001)      << 16;

    value = value | (array[2] & 0b01000000) << (1+8);
    value = value | (array[2] & 0b00010000) << (2+8);
    value = value | (array[2] & 0b00000100) << (3+8);
    value = value | (array[2] & 0b00000001) << (4+8);

    value = value | ((array[3] & 0b01000000) >>3 )<< 8;
    value = value | ((array[3] & 0b00010000) >>2 )<< 8;
    value = value | ((array[3] & 0b00000100) >>1 )<< 8;
    value = value |  (array[3] & 0b00000001)      << 8;

    value = value | (array[4] & 0b01000000) << (1);
    value = value | (array[4] & 0b00010000) << (2);
    value = value | (array[4] & 0b00000100) << (3);
    value = value | (array[4] & 0b00000001) << (4);

    value = value | (array[5] & 0b01000000) >>3;
    value = value | (array[5] & 0b00010000) >>2;
    value = value | (array[5] & 0b00000100) >>1;
    value = value | (array[5] & 0b00000001) ;
    
    if(value & 0x800000) //number is negative
      value = value | 0xff000000;
    else
      value = value & 0x00ffffff;

    return value;
}


/**
 * @brief SPI user event handler.
 * @param event
 */
void spi_event_handler(nrf_drv_spi_evt_t const * p_event,
                       void *                    p_context)
{

    if(nrf_gpio_pin_read(SPI_MISO_PIN)==0)
    {
        NRF_LOG_INFO("Warning! HX711 data not ready"); //if 0: data not ready
        processed_data = -1;
    }
    else
        processed_data = skip_bits(m_rx_buf);

    spi_xfer_done = true;
    
    //NRF_LOG_INFO("Transfer completed. Read: %d", processed_data);
    //NRF_LOG_HEXDUMP_INFO(m_rx_buf, sizeof(m_rx_buf));

}


/**@brief   Sleep until an event is received. */
static void power_manage(void)
{
    (void) sd_app_evt_wait();
}


/*      
      Read data from HX711
*/
int32_t spi_read_data(void)
{
    ret_code_t err_code;

    // Reset rx buffer and transfer done flag
    memset(m_rx_buf, 0, m_rx_length);
    spi_xfer_done = false;

    //note: if clock (MOSI used as clock) stays high for too long,
    //      hx711 shuts down, must finish transmitting on low

    // Send bit bang clock pulses
    
    err_code = nrf_drv_spi_transfer(&spi, m_tx_buf, m_tx_length, m_rx_buf, m_rx_length);
    APP_ERROR_CHECK(err_code);
    

    do
    {
      power_manage();
    }while (spi_xfer_done == false);

    return processed_data;

}

uint32_t spi_power_off(void)
{
    ret_code_t err_code;

    spi_xfer_done = false;

    //note: MOSI line stays on high => HX711 will shut off,
    //      hx711 shuts down, must finish transmitting on low


    //NRF_LOG_INFO("MISO PIN1: %d",nrf_gpio_pin_read(SPI_MISO_PIN));


    // Stop sending on a 1, leaving MOSI (CLK) high, powering off the scale
    err_code = nrf_drv_spi_transfer(&spi, m_tx_buf_pwr_off, m_tx_buf_pwr_off_length, m_rx_buf, m_rx_length);
    APP_ERROR_CHECK(err_code);
    

    //NRF_LOG_INFO("MISO PIN2: %d",nrf_gpio_pin_read(SPI_MISO_PIN));

    do
    {
      power_manage();
    }while (spi_xfer_done == false);


    return 0;

}

void spi_init(uint32_t param_gain_rate)
{
    
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = SPI_SS_PIN;
    spi_config.miso_pin = SPI_MISO_PIN;  
    spi_config.mosi_pin = SPI_MOSI_PIN; 
    spi_config.sck_pin  = SPI_SCK_PIN;
    spi_config.frequency = NRF_DRV_SPI_FREQ_1M;
    spi_config.mode      = NRF_DRV_SPI_MODE_0;
    spi_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;
    spi_config.irq_priority = APP_IRQ_PRIORITY_HIGH;

    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));

    spi_power_off();

    // bit0 and bit1 -> select gain 0 = 32;   1=64;   2=128
    // bit2          -> select rate 0 = 10hz; 1=80hz
    // Gain: 128 => 0x80; 64 => 0xA8; 32 => 0xA0 


    if((param_gain_rate & 3) == 0)
    {
        NRF_LOG_INFO("SPI ADC Gain: 32");
        m_tx_buf[6] = 0xA0;
    }
    if((param_gain_rate & 3) == 1)
    {
        NRF_LOG_INFO("SPI ADC Gain: 64");
        m_tx_buf[6] = 0xA8;
    }
    if((param_gain_rate & 3) == 2)
    {
        NRF_LOG_INFO("SPI ADC Gain: 128");
        m_tx_buf[6] = 0x80;
    }

    //nrf_drv_spi_uninit(&spi);
    //NRF_SPI1->ENABLE = 0;
    
    nrf_gpio_cfg_default(SPI_MISO_PIN);
    nrf_gpio_cfg_default(SPI_MOSI_PIN);


}

void spi_set_gain(uint32_t param_gain_rate)
{

    if((param_gain_rate & 3) == 0)
    {
        NRF_LOG_INFO("SPI ADC Gain: 32");
        m_tx_buf[6] = 0xA0;
    }
    if((param_gain_rate & 3) == 1)
    {
        NRF_LOG_INFO("SPI ADC Gain: 64");
        m_tx_buf[6] = 0xA8;
    }
    if((param_gain_rate & 3) == 2)
    {
        NRF_LOG_INFO("SPI ADC Gain: 128");
        m_tx_buf[6] = 0x80;
    }
}