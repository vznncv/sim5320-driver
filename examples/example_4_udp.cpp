/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example of the UDP usage. It shows current time using NTP protocol.
 *
 * Requirements:
 *
 * - active SIM card with an internet access
 *
 * Pin map:
 *
 * - PA_2 - UART TX (SIM5320E)
 * - PA_3 - UART RX (SIM5320E)
 *
 * Note: to run the example, you should an adjust APN settings in the code.
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

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

static const char *get_reg_mode_name(CellularNetwork::RegistrationType type)
{

    switch (type) {
    case CellularNetwork::C_EREG:
        return "C_EREG";
    case CellularNetwork::C_GREG:
        return "C_GREG";
    case CellularNetwork::C_REG:
        return "C_REG";
    default:
        return "Unknown error";
    }
}

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
 * Get current time from NTP server.
 *
 * @param iface
 * @param ntp_server_address
 * @param ntp_server_port
 * @return
 */
time_t get_current_time(NetworkInterface *iface, const char *ntp_server_address, const int ntp_server_port)
{
    const time_t TIME1970 = (time_t)2208988800UL;
    const int ntp_request_size = 48;
    char ntr_request[ntp_request_size] = { 0 };
    // note: first message byte:
    // - leap indicator: 00 - no leap second adjustment
    // - NTP Version Number: 011
    // - mode: 011 - client
    ntr_request[0] = 0X1B;
    const int ntp_response_size = 48;
    char ntr_response[ntp_response_size] = { 0 };
    UDPSocket socket;
    int response_size;

    // send NTP request and get read response
    CHECK_RET_CODE(socket.open(iface));
    CHECK_RET_CODE(socket.sendto(ntp_server_address, ntp_server_port, ntr_request, ntp_request_size));
    CHECK_RET_CODE(response_size = socket.recv(ntr_response, ntp_response_size));
    CHECK_RET_CODE(socket.close());

    // parse message
    if (response_size < ntp_response_size) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_UNKNOWN), "Invalid NTP response");
    }
    uint32_t seconds_since_1900 = 0;
    for (int i = 40; i < 44; i++) {
        seconds_since_1900 <<= 8;
        seconds_since_1900 += ntr_response[i];
    }

    return (time_t)seconds_since_1900 - TIME1970;
}

int main()
{
    // create driver
    SIM5320 sim5320(PA_2, PA_3);
    // reset and initialize device
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    CellularNetwork *network = sim5320.get_network();
    CellularContext *context = sim5320.get_context();

    // set credential
    //context->set_sim_pin("1234");
    // note: set your APN parameters
    context->set_credentials("internet.mts.ru", "mts", "mts");
    // connect to network
    CHECK_RET_CODE(context->connect()); // note: by default operations is blocking
    printf("The device has connected to network\n");

    // show network registration status and information
    printf("Network information:\n");
    CellularNetwork::registration_params_t reg_param;
    CHECK_RET_CODE(network->get_registration_params(CellularNetwork::C_GREG, reg_param));
    printf("  - registration status: %s/%s\n", get_reg_status_name(reg_param._status), get_radio_access_technology_name(reg_param._act));
    printf("  - ip address: %s\n", context->get_ip_address());

    // UDP demo
    const char *ntp_server_address = "2.pool.ntp.org";
    const int ntp_server_port = 123;

    // make time protocol query
    printf("Make NTP request to \"%s\" ...\n", ntp_server_address);
    time_t current_time = get_current_time(context, ntp_server_address, ntp_server_port);
    printf("Success. Current time: %s\n", ctime(&current_time));
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(context->disconnect());
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
