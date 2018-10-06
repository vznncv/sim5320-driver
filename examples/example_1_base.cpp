/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows device information.
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
        sprintf(error_message, "\"%s\" failed", action_name);
    } else {
        strcpy(error_message, "Device error");
    }
    print_separator();
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err_code), error_message);
}

static const char* SIM_STATE_NAMES_MAP[] = {
    "SimStateReady",
    "SimStatePinNeeded",
    "SimStatePukNeeded",
    "SimStateUnknown"
};

static const char* NETWORK_REG_STATUS_NAMES_MAP[] = {
    "NotRegistered",
    "RegisteredHomeNetwork",
    "SearchingNetwork",
    "RegistrationDenied",
    "Unknown",
    "RegisteredRoaming",
    "RegisteredSMSOnlyHome",
    "RegisteredSMSOnlyRoaming",
    "AttachedEmergencyOnly",
    "RegisteredCSFBNotPreferredHome",
    "RegisteredCSFBNotPreferredRoaming"
};

static const char* NETWORK_RAT_NAMES_MAP[] = {
    "RAT_GSM",
    "RAT_GSM_COMPACT",
    "RAT_UTRAN",
    "RAT_EGPRS",
    "RAT_HSDPA",
    "RAT_HSUPA",
    "RAT_HSDPA_HSUPA",
    "RAT_E_UTRAN",
    "RAT_CATM1",
    "RAT_NB1",
    "RAT_UNKNOWN"
};

// simple led demo
int main()
{
    // create driver
    SIM5320 sim5320(PB_10, PB_11, PB_14, PB_13);
    // start hardware flow control (optionally)
    // check_ret_code(sim5320.start_uart_hw_flow_ctrl(), "configure flow control");
    // reset device (we do it explicitly, as it isn't reseted along with microcontroller)
    check_ret_code(sim5320.reset(), "device resetting");
    // perform basic driver initialization
    check_ret_code(sim5320.init(), "initialization");

    printf("Start device ...\n");
    check_ret_code(sim5320.request_to_start());
    printf("The device has been run\n");

    char str_buff[256];

    // show device information
    CellularInformation* info = sim5320.get_information();
    print_separator();
    printf("Device information\n");
    info->get_manufacturer(str_buff, 256);
    printf("Manufacturer: \"%s\"\n", str_buff);
    info->get_model(str_buff, 256);
    printf("Model: \"%s\"\n", str_buff);
    info->get_revision(str_buff, 256);
    printf("Revision: \"%s\"\n", str_buff);
    info->get_serial_number(str_buff, 256, CellularInformation::IMEI);
    printf("Serial number: \"%s\"\n", str_buff);

    // show sim information
    CellularSIM* sim = sim5320.get_sim();
    CellularSIM::SimState sim_state;
    str_buff[0] = '\0';
    print_separator();
    printf("SIM information");
    sim->get_sim_state(sim_state);
    printf("State: %s\n", SIM_STATE_NAMES_MAP[sim_state]);
    sim->get_iccid(str_buff, 256);
    printf("Serial number: \"%s\"\n", str_buff);
    sim->get_imsi(str_buff);
    printf("IMSI: \"%s\"\n", str_buff);

    // show network information
    int rssi, ber;
    CellularNetwork::RegistrationStatus reg_status;
    CellularNetwork::AttachStatus attach_status;
    CellularNetwork::operator_t cell_operator;
    CellularNetwork::RadioAccessTechnology rat;
    int format;
    str_buff[0] = '\0';
    print_separator();
    printf("Network information\n");
    check_ret_code(sim5320.wait_network_registration(), "network waiting");
    CellularNetwork* network = sim5320.get_network();
    network->get_signal_quality(rssi, ber);
    printf("Signal quality: %d/%d\n", rssi, ber);
    network->get_registration_status(CellularNetwork::C_REG, reg_status);
    printf("Registration status: %s (%d)\n", NETWORK_REG_STATUS_NAMES_MAP[reg_status], reg_status);
    network->get_attach(attach_status);
    printf("Attach status: %d\n", attach_status);
    network->get_operator_params(format, cell_operator);
    printf("Operator format: %d\n", format);
    printf("Operator name: \"%s\"\n", cell_operator.op_long);
    network->get_access_technology(rat);
    printf("Access technology name: \"%s\"\n", NETWORK_RAT_NAMES_MAP[rat]);

    print_separator();
    printf("Stop device ...\n");
    check_ret_code(sim5320.request_to_stop(), "device shutdown");
    printf("The device has been stopped\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
