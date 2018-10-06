/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows TCP usage.
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

/**
 * Helper function to demonstrate http requests usage.
 *
 * @param network
 * @param host
 * @param port
 * @param page_path
 * @return
 */
void http_get_request_demo(NetworkInterface* network, const char* host, int port, const char* page_path)
{
    char str_buff[256];
    nsapi_size_or_error_t ret_val;
    // prepare request
    sprintf(str_buff,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n\r\n",
        page_path, host, port);
    printf("-------------------------------- Request --------------------------------\n");
    printf(str_buff);
    printf("\n");
    // open socket
    TCPSocket socket;
    socket.set_timeout(2000);
    check_ret_code(socket.open(network), "open socket");
    check_ret_code(socket.connect(host, 80), "open connect");
    // send request
    nsapi_size_t request_len = strlen(str_buff);
    nsapi_size_t i = 0;
    while (i < request_len) {
        ret_val = socket.send(str_buff + i, request_len - i);
        if (ret_val < 0) {
            check_ret_code(ret_val, "socket.send");
        } else {
            i += ret_val;
        }
    }
    // read response
    printf("-------------------------------- Response --------------------------------\n");

    while (true) {
        ret_val = socket.recv(str_buff, 255);
        if (ret_val > 0) {
            str_buff[ret_val] = '\0';
            printf("%s", str_buff);
        } else if (ret_val == NSAPI_ERROR_WOULD_BLOCK || ret_val == 0) {
            // end of transmission
            break;
        } else {
            check_ret_code(ret_val, "socket.recv");
        }
    }
    printf("\n");
    printf("-------------------------------------------------------------------------\n");

    // close socket
    check_ret_code(socket.close(), "socket.close");
}

// simple led demo
int main()
{
    // create driver
    SIM5320 sim5320(PB_10, PB_11, PB_14, PB_13);
    // reset device (we do it explicitly, as it isn't reset along with microcontroller)
    check_ret_code(sim5320.reset(), "device resetting");
    // perform basic driver initialization
    check_ret_code(sim5320.init(), "initialization");
    printf("Start device ...\n");
    check_ret_code(sim5320.request_to_start(), "start full functionality mode");
    printf("The device has been run\n");

    // wait network registration (assume that sim can work without PIN)
    check_ret_code(sim5320.wait_network_registration(), "network registration");
    // set APN settings
    CellularNetwork* network = sim5320.get_network();
    check_ret_code(network->set_credentials("internet.mts.ru", "mts", "mts"), "set credentials"); // set credential of the your operator
    check_ret_code(network->set_stack_type(IPV4_STACK)); // set IPv4 usage
    printf("Connect to network ...\n");
    check_ret_code(network->connect(), "network connection");
    printf("Device has connected to network\n");
    // show current settings
    print_separator();
    printf("Device IP: %s\n", network->get_ip_address());

    print_separator();
    // show DNS demo
    const char* server_name = "example.com";
    SocketAddress addr;
    check_ret_code(network->gethostbyname(server_name, &addr), "dns usage example");
    printf("IP address of the \"%s\": %s\n", server_name, addr.get_ip_address());

    // send a simple http request
    print_separator();
    http_get_request_demo(network, server_name, 80, "/");
    print_separator();

    printf("Close network ...\n");
    check_ret_code(network->disconnect(), "network disconnection");
    printf("Network has been closed\n");

    printf("Stop device ...\n");
    check_ret_code(sim5320.request_to_stop(), "device shutdown");
    printf("The device has been stopped\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
