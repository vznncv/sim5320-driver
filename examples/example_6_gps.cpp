/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example shows GPS usage.
 *
 * Pin map:
 *
 * - PB_10 - UART TX (SIM5320E)
 * - PB_11 - UART RX (SIM5320E)
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

#define APP_ERROR(err, message) MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), message)
#define CHECK_RET_CODE(expr)                                                                              \
    {                                                                                                     \
        int err = expr;                                                                                   \
        if (err < 0) {                                                                                    \
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), "Expression \"" #expr "\" failed"); \
        }                                                                                                 \
    }

DigitalOut led(LED2);

#define SEPARATOR_WIDTH 80

void print_separator(const char fill_sep = '=', const int width = SEPARATOR_WIDTH, const char end = '\n')
{
    for (int i = 0; i < width; i++) {
        putchar(fill_sep);
    }
    if (end) {
        putchar(end);
    }
}

void print_header(const char *header, const char left_sep = '-', const char right_sep = '-', const int width = SEPARATOR_WIDTH)
{
    int sep_n = SEPARATOR_WIDTH - strlen(header) - 2;
    sep_n = sep_n < 0 ? 0 : sep_n;
    int sep_l_n = sep_n / 2;
    int sep_r_n = sep_n - sep_l_n;

    print_separator(left_sep, sep_l_n, '\0');
    printf(" %s ", header);
    print_separator(right_sep, sep_r_n);
}

void print_time(time_t *time)
{
    tm parsed_time;
    char str_buf[32];
    gmtime_r(time, &parsed_time);
    strftime(str_buf, 32, "%Y/%m/%d %H:%M:%S (UTC)", &parsed_time);
    printf("%s", str_buf);
}

// simple led demo
int main()
{
    // create driver
    SIM5320 sim5320(PB_10, PB_11);
    // reset and initialize device
    printf("Reset device ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    CellularContext *context = sim5320.get_context();
    // set credential
    //context->set_sim_pin("1234");
    context->set_credentials("internet.mts.ru", "mts", "mts");
    // connect to network
    // note: this operation is optional
    CHECK_RET_CODE(context->connect());
    printf("The device has connected to network\n");

    // GPS demo
    SIM5320GPSDevice *gps = sim5320.get_gps();
    CHECK_RET_CODE(gps->start());
    // set desired GPS accuracy
    CHECK_RET_CODE(gps->set_desired_accuracy(25));
    print_header("Start gps");
    SIM5320GPSDevice::gps_coord_t gps_coord;
    bool has_gps_coord = false;
    int gps_count = 5;
    while (gps_count >= 0) {
        gps->get_coord(has_gps_coord, gps_coord);
        if (has_gps_coord) {
            gps_count--;
            // print gps coordinates
            printf("GPS data:\n");
            printf("  - longitude: %.8f\n", gps_coord.longitude);
            printf("  - latitude: %.8f\n", gps_coord.latitude);
            printf("  - altitude: %.1f\n", gps_coord.altitude);
            printf("  - timestamp: ");
            print_time(&gps_coord.time);
            printf("\n");
        } else {
            printf("Wait gps coordinates...\n");
        }
        wait_ms(5000);
    }
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(gps->stop());
    CHECK_RET_CODE(context->disconnect());
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
