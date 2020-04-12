/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example of the LocationService usage. It shows information about current cellular base station.
 *
 * Requirements:
 *
 * - active SIM card
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

/**
 * Settings
 */
#define MODEM_TX_PIN PD_8
#define MODEM_RX_PIN PD_9
#define MODEM_SIM_PIN ""
#define APP_LED LED2

#define APP_ERROR(err, message) MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), message)
#define CHECK_RET_CODE(expr)                                                           \
    {                                                                                  \
        int err = expr;                                                                \
        if (err < 0) {                                                                 \
            char err_msg[64];                                                          \
            sprintf(err_msg, "Expression \"" #expr "\" failed (error code: %i)", err); \
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), err_msg);        \
        }                                                                              \
    }
;
DigitalOut led(APP_LED);

int main()
{
    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    // reset and initialize device
    printf("Initialize and start modem ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    CHECK_RET_CODE(sim5320.get_device()->set_pin(MODEM_SIM_PIN));
    CHECK_RET_CODE(sim5320.request_to_start());
    printf("Complete\n");

    SIM5320LocationService::station_info_t station_info;
    bool has_res;
    int count = 0;
    SIM5320LocationService *location_service = sim5320.get_location_service();

    while (1) {
        CHECK_RET_CODE(location_service->cell_system_read_info(&station_info, has_res));

        printf("%4i. ", count);
        if (has_res) {
            printf("station info: MCC: %i, LAC: %i, MNC: %i, CID: %i, signal: %i db, network: %s",
                station_info.mcc, station_info.lac, station_info.mnc, station_info.cid, station_info.signal_db,
                station_info.network_type == 0 ? "2g" : "3g");
        } else {
            printf("station isn't found");
        }
        printf("\n");

        ThisThread::sleep_for(5000);
        count++;
        led = !led;
    }
}
