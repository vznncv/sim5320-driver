/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example shows GPS standalone usage.
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

/**
 * Settings.
 */
// Modem settings.
#define MODEM_TX_PIN PD_8
#define MODEM_RX_PIN PD_9
// Led indicator
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

void print_time(time_t *time)
{
    tm parsed_time;
    char str_buf[32];
    gmtime_r(time, &parsed_time);
    strftime(str_buf, 32, "%Y/%m/%d %H:%M:%S (UTC)", &parsed_time);
    printf("%s", str_buf);
}

void print_coord(SIM5320LocationService::coord_t *coord)
{
    printf("GPS data:\n");
    printf("  - longitude: %.8f\n", coord->longitude);
    printf("  - latitude: %.8f\n", coord->latitude);
    printf("  - altitude: %.1f\n", coord->altitude);
    printf("  - timestamp: ");
    print_time(&coord->time);
    printf("\n");
}

int main()
{
    // led and button
    DigitalOut led(APP_LED);
    Timer tm;
    int t;
    SIM5320LocationService::coord_t coord;
    bool has_coord;

    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    SIM5320LocationService *location_service = sim5320.get_location_service();

    // reset and initialize device
    printf("Initialize device ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    // reset GPS settings
    CHECK_RET_CODE(location_service->gps_xtra_set(false));
    CHECK_RET_CODE(location_service->gps_set_accuracy(20));

    // get GPS coordinates
    printf("Measure GPS coordinates ...\n");
    tm.start();
    CHECK_RET_CODE(location_service->gps_locate(&coord, has_coord));
    t = tm.read_ms() / 1000;
    if (has_coord) {
        printf("Coordinates has been resolved witing %i seconds\n", t);
        print_coord(&coord);
    } else {
        printf("Coordinates hasn't beed resolved within %i seconds\n", t);
    }

    printf("Stop ...\n");
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (true) {
        ThisThread::sleep_for(500);
        led = !led;
    }
}
