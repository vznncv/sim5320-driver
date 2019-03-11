/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows SMS usage.
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

class SMSHandler {
public:
    SMSHandler(CellularSMS* sms)
        : _sms_ptr(sms)
    {
    }

    bool show_last_sms()
    {

        nsapi_size_or_error_t ret_val = _sms_ptr->get_sms(_content, CONTENT_SIZE, _phone, PHONE_SIZE, _timestamp, TIMESTAMP_SIZE, NULL);
        bool has_sms = true;
        printf("--sms-- (%d)>>>\n", ret_val);
        if (ret_val == -1) {
            printf("No sms are found.\n");
            has_sms = false;
        } else if (ret_val >= 0) {
            printf("phone: %s\n", _phone);
            printf("timestamp: %s\n", _timestamp);
            printf("sms text:\n%s\n", _content);
        } else {
            printf("Error: %d\n", ret_val);
        }
        printf("--sms--<<<\n");
        return has_sms;
    }

    void show_last_sms_void()
    {
        show_last_sms();
    }

private:
    static const size_t CONTENT_SIZE = 256;
    static const size_t PHONE_SIZE = 32;
    static const size_t TIMESTAMP_SIZE = 32;

    char _content[CONTENT_SIZE];
    char _phone[PHONE_SIZE];
    char _timestamp[TIMESTAMP_SIZE];

    CellularSMS* _sms_ptr;
};

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

    // wait network registration (assume that sim can work without PIN)
    check_ret_code(sim5320.wait_network_registration(), "network registration");

    // SMS demo
    CellularSMS* sms = sim5320.get_sms();
    SMSHandler* sms_halder = new SMSHandler(sms);
    // show all available sms
    print_separator();
    printf("Last message\n");
    sms_halder->show_last_sms();
    // send test sms
    print_separator();
    const char* phone = "+7911xxxyyxx";
    const char* message = "It's test sms message from sim5320\n";
    printf("Send test message\n");
    printf("phone: %s\n", phone);
    printf("message:\n%s\n", message);
    err = sms->send_sms(phone, message, strlen(message));
    if (err < 0) {
        check_ret_code(err, "sms sending");
    }
    printf("Complete!\n");
    print_separator();
    // register callback to print received message
    printf("Wait incoming messages\n");
    sms->set_sms_callback(callback(sms_halder, &SMSHandler::show_last_sms_void));

    while (1) {
        wait(0.5);
        led = !led;
    }
}
