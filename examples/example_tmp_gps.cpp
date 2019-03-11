/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows GPS usage.
 *
 * Pin map:
 *
 * - PC_4 - UART TX (stdout/stderr)
 * - PC_5 - UART RX (stdin)
 * - PB_10 - UART TX (SIM5320E)
 * - PB_11 - UART RX (SIM5320E)
 * - PB_14 - UART RTS (SIM5320E)
 * - PB_13 - UART CTS (SIM5320E)
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

DigitalOut led(LED2);

void print_separator()
{
    printf("====================================================================================\n");
}

void print_time(time_t* time)
{
    tm parsed_time;
    char str_buf[32];
    gmtime_r(time, &parsed_time);
    strftime(str_buf, 32, "%Y/%m/%d %H:%M:%S (UTC)", &parsed_time);
    printf("%s", str_buf);
}

void check_ret_code(int err_code, const char* action_name = NULL)
{
    if (err_code == 0) {
        return;
    }
    char error_message[80];
    if (action_name) {
        sprintf(error_message, "\"%s\" failed with code %d", action_name, err_code);
    } else {
        strcpy(error_message, "Device error");
    }
    print_separator();
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err_code), error_message);
}

// simple led demo
int main()
{
    nsapi_size_or_error_t err = NSAPI_ERROR_OK;
    // create driver
    SIM5320 sim5320(PB_10, PB_11, PB_14, PB_13);
    // reset device (we do it explicitly, as it isn't reset along with microcontroller)
    check_ret_code(sim5320.reset(), "device resetting");
    // perform basic driver initialization
    check_ret_code(sim5320.init(), "initialization");
    check_ret_code(sim5320.start_uart_hw_flow_ctrl(), "start hardware control");
    printf("Start device ...\n");
    check_ret_code(sim5320.request_to_start(), "start full functionality mode");
    printf("The device has been run\n");

    // wait network registration (optionally)
    sim5320.wait_network_registration(10000);
    sim5320.get_network()->set_credentials("internet.mts.ru", "mts", "mts");
    sim5320.get_network()->activate_context();

    // GPS demo
    SIM5320GPSDevice* gps = sim5320.get_gps();
    check_ret_code(gps->start(), "gps startup");
    // set desired GPS accuracy
    gps->set_desired_accuracy(25);
    printf("Start gps\n");
    print_separator();
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
    check_ret_code(gps->stop(), "gps shutdown");
    printf("Stop gps\n");
    check_ret_code(sim5320.request_to_stop(), "device shutdown");
    printf("Shutdown device");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
