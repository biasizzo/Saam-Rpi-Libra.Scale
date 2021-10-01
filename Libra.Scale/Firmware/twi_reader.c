/**
 *
 *   This file contains functions for TWI/I2C communication with MAX77734
 *
 */

#include <stdio.h>

#include "boards.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "app_timer.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"



/* TWI instance ID. */
#define TWI_INSTANCE_ID     0

/* I2C addr for device */
#define I2C_ADDR          (0x48U)


/* Indicates if operation on TWI has ended. */
static volatile bool m_xfer_done = false;

/* TWI instance. */
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

/* Buffer for samples read from temperature sensor. */
uint8_t data_buf[4];


/* Processed data variables */
uint8_t data;

int i = 0;
/**@brief   Sleep until an event is received. */
static void power_manage(void)
{

    (void) sd_app_evt_wait();
}



/**
 * @brief Function for handling data.
 *
 */

__STATIC_INLINE void data_handler()
{
    data = data_buf[0];
}

/**
 * @brief TWI events handler.
 */
void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:

            if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX)
            {
                data_handler();
                //NRF_LOG_INFO("RX complete");
            }


            if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_TX)
            {
                //NRF_LOG_INFO("TX complete");
            }

            m_xfer_done = true;

            //ret_code_t err_code = app_timer_stop(m_timer_twi_timeout);
            //APP_ERROR_CHECK(err_code);

            break;
        case NRF_DRV_TWI_EVT_ADDRESS_NACK :
            NRF_LOG_INFO("I2C ERROR: NACK received after sending the address");
            m_xfer_done = true;
            break;
        case NRF_DRV_TWI_EVT_DATA_NACK  :
            NRF_LOG_INFO("I2C ERROR: NACK received after sending the data byte");
            m_xfer_done = true;
            break;

        default:
            break;
    }
}

/**
 * @brief Function for reading a register.
 */
uint8_t twi_read_register_data(uint8_t register_pointer)
{

    NRF_LOG_INFO("Start I2C reading");

    // Write register address to read
    ret_code_t err_code;
    err_code = nrf_drv_twi_tx(&m_twi, I2C_ADDR, &register_pointer, 1, false);
    APP_ERROR_CHECK(err_code);


    // Wait until TX completed
    m_xfer_done = false;
    do
    {
      power_manage();
    }while (m_xfer_done == false);


    // Read requested register.
    err_code = nrf_drv_twi_rx(&m_twi, I2C_ADDR, data_buf, 1);
    APP_ERROR_CHECK(err_code);

    // Wait until RX completed
    m_xfer_done = false;
    do
    {
        power_manage();
    }while (m_xfer_done == false);


    return data;

}



/**
 * @brief Function for reading a register.
 */
void twi_write_register_data(uint8_t register_pointer, uint8_t data)
{
  //NRF_LOG_INFO("I2C writing %x to %x", data, register_pointer);

    uint8_t tx_data[] = {register_pointer, data};
    //NRF_LOG_INFO("Start I2C writing");
    //NRF_LOG_INFO("Writing %x at %x", data, register_pointer);

    // Write register address to read
    ret_code_t err_code;
    err_code = nrf_drv_twi_tx(&m_twi, I2C_ADDR, tx_data, 2, false);
    APP_ERROR_CHECK(err_code);


    // Wait until TX completed
    m_xfer_done = false;
    do
    {
        power_manage();
    }while (m_xfer_done == false);


}




/**
 * @brief TWI initialization.
 */
void twi_init (void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t twi_config = {
       .scl                = TWI_SCL_PIN,
       .sda                = TWI_SDA_PIN,
       .frequency          = NRF_DRV_TWI_FREQ_100K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };

    err_code = nrf_drv_twi_init(&m_twi, &twi_config, twi_handler, NULL);
    APP_ERROR_CHECK(err_code);
    

    nrf_drv_twi_enable(&m_twi);
    
    
/*

    Set at each read request instead, then back to 0.
    Draws about 50uA.

    twi_write_register_data(0x28, 0x03); //write "BATT pin voltage" to CNFG_CHG_I[7:0] - MUX_SEL
    //NRF_LOG_INFO("reading 0x31, %x", twi_read_register_data(0x31));

*/
    twi_write_register_data(0x28, 0x00); //write "Multiplexer disabled" to CNFG_CHG_I[7:0] - MUX_SEL

    //CNFG_LDO_B (0x31)
    //0x0e ->  Pin-controlled, active low(normal mode when PMLDO pin is low);  LDO enables when nENLDO asserts or when CHGIN is valid.
    //0x04 ->  Forced normal mode;  LDO is forced disabled
    twi_write_register_data(0x31, 0x0e);


    //commenting this powers off the board after a few seconds, but current draw is stopped
    twi_write_register_data(0x08, 0x08); //CNFG_GLBL: nENLDO_MODE=1; slide switch mode


    twi_write_register_data(0x44, 0x01); //enable current sink (for leds)

    twi_write_register_data(0x40, 0b00011100); //enable current sink (for leds) CNFG_SNK1_A
    twi_write_register_data(0x41, 0xff); //enable current sink (for leds) CNFG_SNK1_B
    twi_write_register_data(0x42, 0b00011100); //enable current sink (for leds) CNFG_SNK2_A
    twi_write_register_data(0x43, 0xff); //enable current sink (for leds) CNFG_SNK2_B


    twi_write_register_data(0x21, 0b10100001); //CNFG_CHG_B: CHG_EN=1; 
    twi_write_register_data(0x22, 0b11000001); //CNFG_CHG_C
    twi_write_register_data(0x23, 0b00010000); //CNFG_CHG_D 
    twi_write_register_data(0x24, 0b11111101); //CNFG_CHG_E
    twi_write_register_data(0x25, 0b11111100); //CNFG_CHG_F
    twi_write_register_data(0x26, 0b01010000); //CNFG_CHG_G
    twi_write_register_data(0x27, 0b01010000); //CNFG_CHG_H


// errata 89 -> no effect, resolved on our chip revision?
/*
    nrf_drv_twi_disable( &m_twi );
    nrf_drv_twi_uninit( &m_twi );

    // Nordic work around to prevent burning excess power due to chip errata
    *(volatile uint32_t *)0x40003FFC = 0;
    *(volatile uint32_t *)0x40003FFC;
    *(volatile uint32_t *)0x40003FFC = 1;


    nrf_drv_twi_disable( &m_twi );
    nrf_drv_twi_uninit( &m_twi );
*/
    //nrf_gpio_pin_set(TWI_SCL_PIN);
    //nrf_gpio_pin_clear(TWI_SDA_PIN);



}
