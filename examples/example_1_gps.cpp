/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * GPS usage demo.
 *
 * Pin map:
 *
 * - PC_4 - UART TX (stdout/stderr)
 * - PC_5 - UART RX (stdin)
 * - PB_10 - UART TX (SIM5320E)
 * - PB_11 - UART RX (SIM5320E)
 */

#include "mbed.h"
#include "sim5320_driver.h"
#include <math.h>
#include <time.h>
using sim5320::SIM5320;
using sim5320::gps_coord_t;

DigitalOut led(LED2);

// simple led demo
int main()
{
    SIM5320 sim5320(PB_10, PB_11, PB_14, PB_13);
    // optionally we can enable debug mode. It will duplicate all AT commands to stdout.
    sim5320.debug_on(true);

    // initialize device
    // this action will check that device is ready for communication
    // and reset it into some default state
    int ret_code = sim5320.init();
    if (ret_code) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ret_code), "Fail SIM5320 initialization");
    }
    printf("SIM5320 is ready for work\n");

    printf("Run GPS ...\n");
    int fail_count;
    const int max_fails = 5;
    const float attempt_delay = 5;

    fail_count = 0;
    while (!sim5320.start()) {
        if (fail_count >= max_fails) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ret_code), "Fail to run module");
        }
        printf("Fail to start module!\n");
        wait(attempt_delay);
        fail_count++;
    }

    fail_count = 0;
    while (!sim5320.gps_start()) {
        if (fail_count >= max_fails) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ret_code), "Fail to run GPS");
        }
        printf("Fail to start module!\n");
        wait(attempt_delay);
        fail_count++;
    }

    printf("GPS has been run");

    // print current GPS coordinates
    gps_coord_t gps_coord;
    bool read_res;
    tm* tm_ptr;
    char time_buff[64];
    while (1) {
        read_res = sim5320.gps_get_coord(&gps_coord);
        printf("================================\n");
        if (read_res) {
            printf("Latitude:  %.6f\n", gps_coord.latitude);
            printf("Longitude: %.6f\n", gps_coord.longitude);
            printf("Altitude:  %.6f\n", gps_coord.altitude);
            tm_ptr = gmtime(&gps_coord.time);
            strftime(time_buff, 64, "Timestamp: %Y-%m-%d %H:%M:%S UTC\n", tm_ptr);
            printf(time_buff);
        } else {
            printf("Wait ...\n");
        }
        wait(5.0);
        led = !led;
    }
}
