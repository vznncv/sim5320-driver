/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example of the TCP usage. It uses HTTP to download and show web page.
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
 * Modem settings.
 */
#define MODEM_TX_PIN PD_8
#define MODEM_RX_PIN PD_9
#define MODEM_SIM_PIN ""
#define MODEM_SIM_APN "internet.mts.ru"
#define MODEM_SIM_APN_USERNAME "mts"
#define MODEM_SIM_APN_PASSWORD "mts"

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

static const char *get_reg_status_name(CellularNetwork::RegistrationStatus status)
{
    switch (status) {
    case CellularNetwork::StatusNotAvailable:
        return "StatusNotAvailable";
    case CellularNetwork::NotRegistered:
        return "NotRegistered";
    case CellularNetwork::RegisteredHomeNetwork:
        return "RegisteredHomeNetwork";
    case CellularNetwork::SearchingNetwork:
        return "SearchingNetwork";
    case CellularNetwork::RegistrationDenied:
        return "RegistrationDenied";
    case CellularNetwork::Unknown:
        return "Unknown";
    case CellularNetwork::RegisteredRoaming:
        return "RegisteredRoaming";
    case CellularNetwork::RegisteredSMSOnlyHome:
        return "RegisteredSMSOnlyHome";
    case CellularNetwork::RegisteredSMSOnlyRoaming:
        return "RegisteredSMSOnlyRoaming";
    case CellularNetwork::AttachedEmergencyOnly:
        return "AttachedEmergencyOnly";
    case CellularNetwork::RegisteredCSFBNotPreferredHome:
        return "RegisteredCSFBNotPreferredHome";
    case CellularNetwork::RegisteredCSFBNotPreferredRoaming:
        return "RegisteredCSFBNotPreferredRoaming";
    case CellularNetwork::AlreadyRegistered:
        return "AlreadyRegistered";
    default:
        return "Unknown error";
    }
}

static const char *get_radio_access_technology_name(CellularNetwork::RadioAccessTechnology rat)
{
    switch (rat) {
    case CellularNetwork::RAT_GSM:
        return "RAT_GSM";
    case CellularNetwork::RAT_GSM_COMPACT:
        return "RAT_GSM_COMPACT";
    case CellularNetwork::RAT_UTRAN:
        return "RAT_UTRAN";
    case CellularNetwork::RAT_EGPRS:
        return "RAT_EGPRS";
    case CellularNetwork::RAT_HSDPA:
        return "RAT_HSDPA";
    case CellularNetwork::RAT_HSUPA:
        return "RAT_HSUPA";
    case CellularNetwork::RAT_HSDPA_HSUPA:
        return "RAT_HSDPA_HSUPA";
    case CellularNetwork::RAT_E_UTRAN:
        return "RAT_E_UTRAN";
    case mbed::CellularNetwork::RAT_CATM1:
        return "RAT_CATM1";
    case CellularNetwork::RAT_NB1:
        return "RAT_NB1";
    case CellularNetwork::RAT_UNKNOWN:
        return "RAT_UNKNOWN";
    case CellularNetwork::RAT_MAX:
        return "RAT_MAX";
    default:
        return "Unknown error";
    }
}

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
 * Helper function to print result of the http request.
 *
 * @param network
 * @param host
 * @param port
 * @param page_path
 * @return
 */
void print_http_page(NetworkInterface *network, const char *host, int port, const char *page_path)
{
    char str_buff[256];
    nsapi_size_or_error_t ret_val;
    // prepare request
    sprintf(str_buff,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n\r\n",
        page_path, host, port);
    print_header("Request");
    puts(str_buff);
    printf("\n");
    // open socket
    TCPSocket socket;
    SocketAddress address;
    CHECK_RET_CODE(network->gethostbyname(host, &address));
    address.set_port(port);
    socket.set_timeout(2000);
    CHECK_RET_CODE(socket.open(network));
    CHECK_RET_CODE(socket.connect(address));
    // send request
    nsapi_size_t request_len = strlen(str_buff);
    nsapi_size_t i = 0;
    while (i < request_len) {
        ret_val = socket.send(str_buff + i, request_len - i);
        if (ret_val < 0) {
            APP_ERROR(ret_val, "socket::send failed");
        } else {
            i += ret_val;
        }
    }
    // read response
    print_header("Response");

    while (true) {
        ret_val = socket.recv(str_buff, 255);
        if (ret_val > 0) {
            str_buff[ret_val] = '\0';
            printf("%s", str_buff);
        } else if (ret_val == NSAPI_ERROR_WOULD_BLOCK || ret_val == 0) {
            // end of transmission
            break;
        } else {
            CHECK_RET_CODE(ret_val);
        }
    }
    printf("\n");

    // close socket
    CHECK_RET_CODE(socket.close());
}

int main()
{
    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    char buf[256];
    SocketAddress address;
    // reset and initialize device
    printf("Initialize modem ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    CellularNetwork *network = sim5320.get_network();
    CellularContext *context = sim5320.get_context();

    // set credential
    if (strlen(MODEM_SIM_PIN) > 0) {
        CHECK_RET_CODE(sim5320.get_device()->set_pin(MODEM_SIM_PIN));
    }
    // set APN settings
    context->set_credentials(MODEM_SIM_APN, MODEM_SIM_APN_USERNAME, MODEM_SIM_APN_PASSWORD);
    // connect to network
    CHECK_RET_CODE(context->connect()); // note: by default operations is blocking
    printf("The device has connected to network\n");

    // show network registration status and information
    printf("Network information:\n");
    CellularNetwork::registration_params_t reg_param;
    CHECK_RET_CODE(network->get_registration_params(CellularNetwork::C_GREG, reg_param));
    printf("  - registration status: %s/%s\n", get_reg_status_name(reg_param._status), get_radio_access_technology_name(reg_param._act));
    CHECK_RET_CODE(context->get_ip_address(&address));
    printf("  - ip address: %s\n", address.get_ip_address());

    // DNS demo
    print_header("DNS demo");
    const char host_num = 3;
    const char *hosts[host_num] = {
        "www.wikipedia.org",
        "google.com",
        "example.com"
    };
    for (int i = 0; i < host_num; i++) {
        context->gethostbyname(hosts[i], &address);
        printf("%s -> %s\n", hosts[i], address.get_ip_address());
    }
    // TCP demo
    print_header("TCP demo");
    print_http_page(context, "artscene.textfiles.com", 80, "/asciiart/dragon.txt");
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(context->disconnect());
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        ThisThread::sleep_for(500);
        led = !led;
    }
}
