/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows common device information.
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
#define APP_LED LED2

#define CHECK_RET_CODE(expr)                                                           \
    {                                                                                  \
        int err = expr;                                                                \
        if (err < 0) {                                                                 \
            char err_msg[64];                                                          \
            sprintf(err_msg, "Expression \"" #expr "\" failed (error code: %i)", err); \
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), err_msg);        \
        }                                                                              \
    }

DigitalOut led(APP_LED);

int main()
{
    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    // reset and initialize device
    printf("Initialize modem ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());

    // get device information without active mode
    const size_t buf_size = 256;
    char buf[buf_size];
    int err;
    CellularInformation *cellular_information = sim5320.get_information();

    printf("Cellular device information:\n");
    CHECK_RET_CODE(cellular_information->get_manufacturer(buf, buf_size));
    printf("  - manufacturer:           %s\n", buf);
    CHECK_RET_CODE(cellular_information->get_model(buf, buf_size));
    printf("  - model:                  %s\n", buf);
    CHECK_RET_CODE(cellular_information->get_revision(buf, buf_size));
    printf("  - revision:               %s\n", buf);
    CHECK_RET_CODE(cellular_information->get_serial_number(buf, buf_size, CellularInformation::SN));
    printf("  - serial number (SN):   %s\n", buf);
    CHECK_RET_CODE(cellular_information->get_serial_number(buf, buf_size, CellularInformation::IMEI));
    printf("  - serial number (IMEI):   %s\n", buf);
    err = cellular_information->get_imsi(buf, buf_size);
    if (err) {
        strcpy(buf, "N/A");
    }
    printf("  - IMSI:                   %s\n", buf);
    err = cellular_information->get_iccid(buf, buf_size);
    if (err) {
        strcpy(buf, "N/A");
    }
    printf("  - ICCID:                  %s\n", buf);

    printf("Complete!\n");

    while (1) {
        ThisThread::sleep_for(500);
        led = !led;
    }
}
