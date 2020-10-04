/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example of the TimeServer usage. It shows current time using requests to http servers.
 *
 * Requirements:
 *
 * - active SIM card with an internet access
 *
 * Note: to run the example, you should adjust APN settings.
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
#define MODEM_SIM_APN "internet.mts.ru"
#define MODEM_SIM_APN_USERNAME "mts"
#define MODEM_SIM_APN_PASSWORD "mts"
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

/**
 * Main code
 */

static DigitalOut led(LED2);

int main()
{
    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    // reset and initialize device
    printf("Initialize modem ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.network_set_params(MODEM_SIM_PIN, MODEM_SIM_APN, MODEM_SIM_APN_USERNAME, MODEM_SIM_APN_PASSWORD));
    CHECK_RET_CODE(sim5320.network_up());
    printf("The device has connected to network\n");

    // check time service
    print_header("time service demo");
    SIM5320TimeService *time_service = sim5320.get_time_service();
    printf("Sync time ...\n");
    CHECK_RET_CODE(time_service->sync_time());
    time_t current_time;
    CHECK_RET_CODE(time_service->get_time(&current_time));
    printf("Success. Current time: %s\n", ctime(&current_time));
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(sim5320.network_down());
    printf("Complete!\n");

    while (1) {
        ThisThread::sleep_for(500ms);
        led = !led;
    }
}
