/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example shows GPS usage with cold and hot startup.
 */

#include <math.h>
#include <time.h>

#include "mbed.h"

#include "sim5320_driver.h"

using mbed::chrono::milliseconds_u32;
using namespace sim5320;

/**
 * Settings
 */
#define MODEM_TX_PIN PD_8
#define MODEM_RX_PIN PD_9
#define APP_LED LED2

/**
 * Helper functions
 */
#define APP_ERROR(err, message) MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), message)
static int _check_ret_code(int res, const char *expr)
{
    static char err_msg[128];
    if (res < 0) {
        snprintf(err_msg, 128, "Expression \"%s\" failed (error code: %i)", expr, res);
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, res), err_msg);
    }
    return res;
}
#define CHECK_RET_CODE(expr) _check_ret_code(expr, #expr)

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

template <class T>
int second_count(T d)
{
    return (std::chrono::duration_cast<std::chrono::seconds>(d)).count();
}

/**
 * Main code
 */

static DigitalOut led(APP_LED);

int main()
{
    Timer tm;
    std::chrono::microseconds t;
    const milliseconds_u32 t_gps_delay = 30s;
    const milliseconds_u32 t_gps_cold = 32s;
    const milliseconds_u32 t_gps_hot = 2s;
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
    CHECK_RET_CODE(location_service->gps_set_accuracy(50));
    CHECK_RET_CODE(location_service->gps_clear_data());

    // first GPS startup
    print_header("GPS cold startup");
    tm.start();
    CHECK_RET_CODE(location_service->gps_start(SIM5320LocationService::GPS_MODE_STANDALONE, SIM5320LocationService::GPS_STARTUP_MODE_COLD));
    has_coord = false;
    while (!has_coord) {
        ThisThread::sleep_for(2000ms);
        CHECK_RET_CODE(location_service->gps_read_coord(&coord, has_coord));
        if (!has_coord) {
            t = tm.elapsed_time();
            if (t > t_gps_cold) {
                printf("Cannot get GPS coordinates during %i seconds ...\n", second_count(t));
            } else {
                printf("Wait GPS coordinates %i seconds ...\n", second_count(t));
            }
        }
    }
    print_coord(&coord);
    CHECK_RET_CODE(location_service->gps_stop());

    // check hot startup
    print_header("GPS hot startup");
    for (int i = 0; i < 3; i++) {
        printf("Delay before start %i ...\n", second_count(t_gps_delay));
        ThisThread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(t_gps_delay));
        printf("Start GPS\n");
        tm.reset();
        CHECK_RET_CODE(location_service->gps_start(SIM5320LocationService::GPS_MODE_STANDALONE, SIM5320LocationService::GPS_STARTUP_MODE_COLD));
        has_coord = false;
        while (!has_coord) {
            ThisThread::sleep_for(2000ms);
            CHECK_RET_CODE(location_service->gps_read_coord(&coord, has_coord));
            if (!has_coord) {
                t = tm.elapsed_time();
                if (t > t_gps_hot) {
                    printf("Cannot get GPS coordinates during %i seconds ...\n", second_count(t));
                } else {
                    printf("Wait GPS coordinates %i seconds ...\n", second_count(t));
                }
            }
        }
        print_coord(&coord);
        printf("Stop GPS\n");
        CHECK_RET_CODE(location_service->gps_stop());
    }

    printf("Stop ...\n");
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (true) {
        ThisThread::sleep_for(500ms);
        led = !led;
    }
}
