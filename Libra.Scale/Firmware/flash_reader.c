
/** @file
 *
 * @brief fstorage example main file.
 *
 * This example showcases fstorage usage.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nrf.h"
#include "nrf_soc.h"
#include "nordic_common.h"
#include "boards.h"
#include "app_timer.h"
#include "app_util.h"
#include "nrf_fstorage.h"
#include "app_error.h"
#include "nrf_soc.h"
#include "sdk_config.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_fstorage_sd.h"


#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define CONFIGURED_ADDR   0x32000
#define CONFIGURED_VALUE  0xbb      //value that confirms settings have been written at least once
#define RT_ADDR           0x32004
#define TO_ADDR           0x32008
#define CF_ADDR           0x3200c
#define RI_ADDR           0x32010
#define GR_ADDR           0x32014
#define PASS_ADDR         0x32018
#define NAME_ADDR         0x32040

static void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt);


NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) =
{
    /* Set a handler for fstorage events. */
    .evt_handler = fstorage_evt_handler,

    /* These below are the boundaries of the flash space assigned to this instance of fstorage.
     * You must set these manually, even at runtime, before nrf_fstorage_init() is called.
     * The function nrf5_flash_end_addr_get() can be used to retrieve the last address on the
     * last page of flash available to write data. */
    .start_addr = 0x32000,
    .end_addr   = 0x32fff,
};


/**@brief   Helper function to obtain the last address on the last page of the on-chip flash that
 *          can be used to write user data.
 */
static uint32_t nrf5_flash_end_addr_get()
{
    uint32_t const bootloader_addr = NRF_UICR->NRFFW[0];
    uint32_t const page_sz         = NRF_FICR->CODEPAGESIZE;
    uint32_t const code_sz         = NRF_FICR->CODESIZE;

    return (bootloader_addr != 0xFFFFFFFF ?
            bootloader_addr : (code_sz * page_sz));
}




/**@brief   Sleep until an event is received. */
static void power_manage(void)
{

    (void) sd_app_evt_wait();

}


static void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt)
{
    if (p_evt->result != NRF_SUCCESS)
    {
        NRF_LOG_INFO("--> Event received: ERROR while executing an fstorage operation.");
        return;
    }

    switch (p_evt->id)
    {
        case NRF_FSTORAGE_EVT_WRITE_RESULT:
        {
            NRF_LOG_INFO("--> Event received: wrote %d bytes at address 0x%x.",
                         p_evt->len, p_evt->addr);
        } break;

        case NRF_FSTORAGE_EVT_ERASE_RESULT:
        {
            NRF_LOG_INFO("--> Event received: erased %d page from address 0x%x.",
                         p_evt->len, p_evt->addr);
        } break;

        default:
            break;
    }
}


static void print_flash_info(nrf_fstorage_t * p_fstorage)
{
    NRF_LOG_INFO("========| flash info |========");
    NRF_LOG_INFO("erase unit: \t%d bytes",      p_fstorage->p_flash_info->erase_unit);
    NRF_LOG_INFO("program unit: \t%d bytes",    p_fstorage->p_flash_info->program_unit);
    NRF_LOG_INFO("==============================");
}


void wait_for_flash_ready(nrf_fstorage_t const * p_fstorage)
{
    /* While fstorage is busy, sleep and wait for an event. */
    // note: fstorage_is_busy will always return busy if called from BLE event
    while (nrf_fstorage_is_busy(p_fstorage))
    {
        power_manage();
    }
}


static uint32_t round_up_u32(uint32_t len)
{
    if (len % sizeof(uint32_t))
    {
        return (len + sizeof(uint32_t) - (len % sizeof(uint32_t)));
    }

    return len;
}



// --------------------- RW functions

static uint32_t fstorage_read_int(uint32_t addr)
{
    ret_code_t  rc;
    uint8_t     array_data[4];

    rc = nrf_fstorage_read(&fstorage, addr, array_data, 4);
    if (rc != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_fstorage_read() returned: %s", nrf_strerror_get(rc));
        return 0;
    }
        //read BIG ENDIAN array data and return it as int

        //NRF_LOG_INFO("reading address %x:",addr);
        //NRF_LOG_INFO("%x %x %x %x",array_data[0],array_data[1],array_data[2],array_data[3]);
        return array_data[0] | (array_data[1] << 8) | (array_data[2] << 16) | (array_data[3] << 24);
}

static void fstorage_read_array(uint32_t addr, uint8_t * buffer, uint32_t len)
{
    ret_code_t  rc;

    rc = nrf_fstorage_read(&fstorage, addr, buffer, len);
    if (rc != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_fstorage_read() returned: %s", nrf_strerror_get(rc));
        return;
        
    }
        //NRF_LOG_INFO("reading address %x:",addr);
        //NRF_LOG_HEXDUMP_INFO(buffer, len);
        
}

static void fstorage_write(uint32_t addr, void const * p_data, uint32_t len)
{
    /* The following code snippet make sure that the length of the data we are writing to flash
     * is a multiple of the program unit of the flash peripheral (4 bytes).
     *
     */
    len = round_up_u32(len);

    ret_code_t rc = nrf_fstorage_write(&fstorage, addr, p_data, len, NULL);
    APP_ERROR_CHECK(rc);
    if (rc != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_fstorage_write() returned: %s", nrf_strerror_get(rc));
    }

    wait_for_flash_ready(&fstorage);
    
}


static void fstorage_erase(uint32_t addr, uint32_t pages_cnt)
{
    ret_code_t rc = nrf_fstorage_erase(&fstorage, addr, pages_cnt, NULL);
    APP_ERROR_CHECK(rc);
    if (rc != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_fstorage_erase() returned: %s\r\n", nrf_strerror_get(rc));
    }

    wait_for_flash_ready(&fstorage);
}

// --------------------- RW functions







void fstorage_save( uint32_t * param_refresh_time,
                    uint32_t * param_refresh_notify,
                    uint32_t * param_timeout,
                     int32_t * param_correction_factor,
                    uint32_t * param_gain_rate,
                    char  * param_password,
                    char  * param_name )
{
        static uint8_t configured_init[4] = {CONFIGURED_VALUE,0,0,0};
        static uint8_t rt_write[4] __attribute__((aligned(4)))= {0};  //must be aligned due to nrf_fstorage_write requirements
        static uint8_t to_write[4] __attribute__((aligned(4)))= {0};
        static uint8_t cf_write[4] __attribute__((aligned(4)))= {0};
        static uint8_t ri_write[4] __attribute__((aligned(4)))= {0};
        static uint8_t gr_write[4] __attribute__((aligned(4)))= {0};


        memcpy(&rt_write, param_refresh_time, 4);
        memcpy(&to_write, param_timeout, 4);
        memcpy(&cf_write, param_correction_factor, 4);
        memcpy(&ri_write, param_refresh_notify, 4);
        memcpy(&gr_write, param_gain_rate, 4);

/*
        NRF_LOG_INFO("rt: %d", *param_refresh_time);
        NRF_LOG_HEXDUMP_INFO(rt_write, 4);
        NRF_LOG_INFO("to: %d", *param_timeout);
        NRF_LOG_HEXDUMP_INFO(to_write, 4);
        NRF_LOG_INFO("cf: %d", *param_correction_factor);
        NRF_LOG_HEXDUMP_INFO(cf_write, 4);

        NRF_LOG_INFO("password %s", param_password);
        NRF_LOG_INFO("name %s", param_name);
*/

        fstorage_erase(CONFIGURED_ADDR, 1);           //erase flash first

        fstorage_write(CONFIGURED_ADDR, &configured_init, 4);
        fstorage_write(RT_ADDR, &rt_write, 4);
        fstorage_write(TO_ADDR, &to_write, 4);
        fstorage_write(CF_ADDR, &cf_write, 4);
        fstorage_write(RI_ADDR, &ri_write, 4);
        fstorage_write(GR_ADDR, &gr_write, 4);
        fstorage_write(PASS_ADDR, param_password, 40);
        fstorage_write(NAME_ADDR, param_name, 40);
/*
        static char test[40] = {0}; 
        NRF_LOG_HEXDUMP_INFO(param_name, 40);
        fstorage_read_array(NAME_ADDR,test,40);
        NRF_LOG_INFO("name readback %s", test);
        NRF_LOG_HEXDUMP_INFO(test, 40);
*/      
        
}


void fstorage_init( uint32_t * param_refresh_time,
                    uint32_t * param_refresh_notify,
                    uint32_t * param_timeout,
                     int32_t * param_correction_factor,
                    uint32_t * param_gain_rate,
                     char    * param_password,
                     char    * param_name )
{
    ret_code_t rc;
    nrf_fstorage_api_t * p_fs_api;

    //NRF_LOG_INFO("end addr %d",nrf5_flash_end_addr_get());
    NRF_LOG_INFO("Initializing nrf_fstorage_sd implementation...");
    /* Initialize an fstorage instance using the nrf_fstorage_sd backend.
     * nrf_fstorage_sd uses the SoftDevice to write to flash. This implementation can safely be
     * used whenever there is a SoftDevice, regardless of its status (enabled/disabled). */
    p_fs_api = &nrf_fstorage_sd;

    rc = nrf_fstorage_init(&fstorage, p_fs_api, NULL);
    APP_ERROR_CHECK(rc);

    //print_flash_info(&fstorage);

    /* It is possible to set the start and end addresses of an fstorage instance at runtime.
     * They can be set multiple times, should it be needed. The helper function below can
     * be used to determine the last address on the last page of flash memory available to
     * store data. */
    (void) nrf5_flash_end_addr_get();

    //====== uncomment to simulate first run ======
    //fstorage_erase(CONFIGURED_ADDR, 1);


    //check if reading for the first time
    //if yes => write defaults
    if(fstorage_read_int(CONFIGURED_ADDR) != CONFIGURED_VALUE)
    {
        NRF_LOG_INFO("First bootup, writing default settings to flash");
        //RT=0062 TO=10 CF=-0119

        static uint8_t configured_init[4] = {CONFIGURED_VALUE,0,0,0};
        static uint8_t rt_init[4] __attribute__((aligned(4)))= {0xc8,0x00,0x00,0x00};   //c8 = 200  //must be aligned due to nrf_fstorage_write legacy requirements
        static uint8_t to_init[4] __attribute__((aligned(4)))= {0x0a,0x00,0x00,0x00};   //0A = 10
        static uint8_t cf_init[4] __attribute__((aligned(4)))= {0x48,0xf4,0xff,0xff};   //FF FF FF 89 = -3000
        static uint8_t ri_init[4] __attribute__((aligned(4)))= {0xe8,0x03,0x00,0x00};   //3e8 = 1000
        static uint8_t gr_init[4] __attribute__((aligned(4)))= {0x02,0x00,0x00,0x00};   //gain rate default = 1 (rate 10, gain 64)
                                                                                       

        static char    password_init[40] = "0000";
        static char    name_init[40] = "Libra";

        fstorage_write(CONFIGURED_ADDR, &configured_init, 4);
        fstorage_write(RT_ADDR, &rt_init, 4);
        fstorage_write(TO_ADDR, &to_init, 4);
        fstorage_write(CF_ADDR, &cf_init, 4);
        fstorage_write(RI_ADDR, &ri_init, 4);
        fstorage_write(GR_ADDR, &gr_init, 4);

        fstorage_write(PASS_ADDR, &password_init, 40);
        fstorage_write(NAME_ADDR, &name_init, 40);

    }

    NRF_LOG_INFO("Reading settings from flash");
    *param_refresh_time      = fstorage_read_int(RT_ADDR);
    *param_timeout           = fstorage_read_int(TO_ADDR);
    *param_correction_factor = fstorage_read_int(CF_ADDR);
    *param_refresh_notify    = fstorage_read_int(RI_ADDR);
    *param_gain_rate         = fstorage_read_int(GR_ADDR);

    fstorage_read_array(PASS_ADDR,param_password,40);
    fstorage_read_array(NAME_ADDR,param_name,40);

    //uint8_t buf[256]={0};
    //fstorage_read_array(PASS_ADDR,buf,256);

/*
    uint32_t data2=fstorage_read_int(RT_ADDR);
    NRF_LOG_INFO("data2 %x",data2);
*/

}


void fstorage_uninit()
{
    ret_code_t rc;
    nrf_fstorage_api_t * p_fs_api;
    NRF_LOG_INFO("Un-initializing nrf_fstorage_sd implementation...");
    rc = nrf_fstorage_uninit(&fstorage, NULL);
    APP_ERROR_CHECK(rc);
}

