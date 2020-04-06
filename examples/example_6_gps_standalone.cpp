/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example shows GPS usage.
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

/**
 * Modem settings.
 */
#define MODEM_TX_PIN PD_8
#define MODEM_RX_PIN PD_9

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

DigitalOut led(LED2);
InterruptIn button(BUTTON1);

struct click_detector_t {
    volatile int counter;

    click_detector_t(int counter = 0)
        : counter(counter)
    {
    }

    void reset()
    {
        counter = 0;
    }

    void click()
    {
        counter++;
    }
};

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

int main()
{
    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    // reset and initialize device
    printf("Initialize device ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    // GPS demo
    SIM5320GPSDevice *gps = sim5320.get_gps();
    CHECK_RET_CODE(gps->start(SIM5320GPSDevice::STANDALONE_MODE));
    // set desired GPS accuracy
    CHECK_RET_CODE(gps->set_desired_accuracy(150));
    print_header("Start gps");
    SIM5320GPSDevice::gps_coord_t gps_coord;
    bool has_gps_coord = false;
    click_detector_t click_detector;
    button.rise(callback(&click_detector, &click_detector_t::click));

    printf("Wait gps coordinates. Click button to stop\n");
    while (click_detector.counter == 0) {
        gps->get_coord(has_gps_coord, gps_coord);
        if (has_gps_coord) {
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
        ThisThread::sleep_for(5000);
    }
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        ThisThread::sleep_for(500);
        led = !led;
    }
}
