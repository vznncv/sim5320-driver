/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * SMS usage demo.
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
#include "sim5320_driver.h"
#include <math.h>
#include <time.h>
using sim5320::SIM5320;

DigitalOut led(LED2);

const char* network_type_to_name(SIM5320::NetworkType network_type)
{
    switch (network_type) {
    case sim5320::SIM5320::NETWORK_TYPE_NO_SERVICE:
        return "NO SERVICE";
    case sim5320::SIM5320::NETWORK_TYPE_GSM:
        return "GSM";
    case sim5320::SIM5320::NETWORK_TYPE_GPRS:
        return "GPRS";
    case sim5320::SIM5320::NETWORK_TYPE_EGPRS:
        return "EGPRS";
    case sim5320::SIM5320::NETWORK_TYPE_WCDMA:
        return "WCDMA";
    case sim5320::SIM5320::NETWORK_TYPE_HSPA:
        return "HSPA";
    default:
        return "Unknown";
    }
}

void check_ftp(bool done)
{
    if (!done) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_INITIALIZATION_FAILED), "Fail ftp operation");
    }
}

void print_list_item(const char* name)
{
    printf("- %s\n", name);
}

// simple led demo
int main()
{
    bool done;
    int ret_code;
    char buff[64];
    SIM5320 sim5320(PB_10, PB_11, PB_14, PB_13);
    // optionally we can enable debug mode. It will duplicate all AT commands to stdout.
    sim5320.debug_on(true);
    // reset device (for debug purposes, when we reload board, but don't reload SIM5320)
    done = sim5320.reset();
    if (!done) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_INITIALIZATION_FAILED), "Fail to reset SIM5320");
    }
    // initialize device
    ret_code = sim5320.init();
    if (ret_code) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ret_code), "Fail SIM5320 initialization");
    }
    // start hardware control
    done = sim5320.start_uart_hw_flow_ctrl();
    if (!done) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ret_code), "Fail so start hardware control initialization");
    }
    printf("SIM5320 is ready for work\n");
    // run device
    done = sim5320.start();
    if (!done) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, done), "Fail to run SIM5320");
    }
    // wait network registration
    done = sim5320.network_wait_registration();
    if (!done) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, done), "Fail network registration");
    }
    // print network information
    printf("================================\n");
    printf("Network type:    %s\n", network_type_to_name(sim5320.network_get_type()));
    printf("Signal strength: %.1f dBm\n", sim5320.network_get_signal_strength());
    sim5320.network_get_operator_name(buff);
    printf("Operator name:  \"%s\"\n", buff);
    printf("================================\n");

    // get ip address
    printf("Get IP address ...\n");
    done = sim5320.network_configure_context("internet.mts.ru", "mts", "mts");
    if (!done) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, done), "Fail to configure internet access");
    }
    sim5320.network_get_device_ip_address(buff);
    printf("Device IP address: %s\n", buff);
    printf("================================\n");

    // ftp demo
    check_ftp(sim5320.ftp_connect("test.rebex.net", 21, SIM5320::FTP_PROTOCOL_FTP, "demo", "password"));
    check_ftp(sim5320.ftp_get_cwd(buff));
    printf("Current ftp directory: %s\n", buff);
    printf("Directory content:\n");
    check_ftp(sim5320.ftp_list_dir("", callback(print_list_item)));
    check_ftp(sim5320.ftp_close());

    while (1) {

        wait(0.5);
        led = !led;
    }
}
