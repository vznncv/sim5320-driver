/**
 * Network registration test case.
 *
 * The test requires active SIM card and an aviable network.
 */

#include "greentea-client/test_env.h"
#include "math.h"
#include "mbed.h"
#include "rtos.h"
#include "sim5320_driver.h"
#include "string.h"
#include "unity.h"
#include "utest.h"

using namespace utest::v1;
using namespace sim5320;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static sim5320::SIM5320 *modem;

utest::v1::status_t test_setup_handler(const size_t number_of_cases)
{
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX);
    modem->reset();
    // set PIN if we have it
    const char *pin = TOSTRING(MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN);
    if (strlen(pin) > 0) {
        modem->get_device()->set_pin(pin);
    }

    return greentea_test_setup_handler(number_of_cases);
}

void test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    delete modem;
    return greentea_test_teardown_handler(passed, failed, failure);
}

utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    // reset modem
    modem->init();
    return greentea_case_setup_handler(source, index_of_case);
}

static bool not_empty(const char *str)
{
    return strlen(str) > 0;
}

void test_network_regisration()
{
    int err;
    CellularDevice *cellular_device = modem->get_device();
    CellularNetwork *cellular_network = modem->get_network();

    // start modem
    err = modem->request_to_start();
    TEST_ASSERT_EQUAL(0, err);
    // attach to network
    CellularNetwork::AttachStatus attach_status;
    cellular_network->set_attach();
    TEST_ASSERT_EQUAL(0, err);
    for (int i = 0; i < 30; i++) {
        cellular_network->get_attach(attach_status);
        if (attach_status == CellularNetwork::Attached) {
            break;
        }
        wait_ms(1000);
    }

    // check registration parameter
    CellularNetwork::registration_params_t reg_param;
    err = cellular_network->get_registration_params(CellularNetwork::C_GREG, reg_param);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_NOT_EQUAL(CellularNetwork::StatusNotAvailable, reg_param._status);
    TEST_ASSERT_NOT_EQUAL(CellularNetwork::NotRegistered, reg_param._status);
    TEST_ASSERT_NOT_EQUAL(CellularNetwork::Unknown, reg_param._status);
    TEST_ASSERT_NOT_EQUAL(CellularNetwork::RAT_UNKNOWN, reg_param._act);

    // check signal parameters
    int signal_rssi = -10;
    int signal_ber = -10;
    cellular_network->get_signal_quality(signal_rssi, &signal_ber);
    TEST_ASSERT_EQUAL(0, err);

    // check operator parameters
    CellularNetwork::operator_t active_operator;
    int operator_format;
    err = cellular_network->get_operator_params(operator_format, active_operator);
    TEST_ASSERT_EQUAL(0, err);
    switch (operator_format) {
    case 0:
        TEST_ASSERT(not_empty(active_operator.op_long));
        break;
    case 1:
        TEST_ASSERT(not_empty(active_operator.op_short));
        break;
    default:
        TEST_ASSERT(not_empty(active_operator.op_num));
        break;
    }

    // check list of available operators
    CellularNetwork::operList_t nw_operator_list;
    CellularNetwork::operator_t *nw_operator_ptr;
    int ops_count = 0, actual_ops_count = 0;
    err = cellular_network->scan_plmn(nw_operator_list, ops_count);
    TEST_ASSERT_EQUAL(0, err);
    for (nw_operator_ptr = nw_operator_list.get_head(); nw_operator_ptr != NULL; nw_operator_ptr = nw_operator_ptr->next) {
        TEST_ASSERT(not_empty(nw_operator_ptr->op_long));
        TEST_ASSERT(not_empty(nw_operator_ptr->op_short));
        TEST_ASSERT(not_empty(nw_operator_ptr->op_num));
        actual_ops_count++;
    }
    TEST_ASSERT_EQUAL(ops_count, actual_ops_count);

    // check list of all operators from SIM5320 memory
    CellularNetwork::operator_names_t *nw_operator_name_ptr;
    CellularNetwork::operator_names_list nw_operator_name_list;
    err = cellular_network->get_operator_names(nw_operator_name_list);
    TEST_ASSERT_EQUAL(0, err);
    for (nw_operator_name_ptr = nw_operator_name_list.get_head(); nw_operator_name_ptr != NULL; nw_operator_name_ptr = nw_operator_name_ptr->next) {
        if (!not_empty(nw_operator_name_ptr->alpha)) {
            nw_operator_name_ptr->alpha[2] = 'q';
        }
        // TEST_ASSERT(not_empty(nw_operator_name_ptr->alpha));
        TEST_ASSERT(not_empty(nw_operator_name_ptr->numeric));
    }

    // detach from network
    err = cellular_network->detach();
    TEST_ASSERT_EQUAL(0, err);
    // stop modem (CellularDevise::shutdown)
    err = modem->request_to_stop();
    TEST_ASSERT_EQUAL(0, err);
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
Case cases[] = {
    SIM5320Case(test_network_regisration),
};
Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // host handshake
    // note: should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(180, "default_auto");
    // run tests
    return !Harness::run(specification);
}
