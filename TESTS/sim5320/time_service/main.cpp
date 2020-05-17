/**
 * Time service client test case.
 *
 * The test requires:
 * - active SIM card
 * - an aviable network.
 */
#include "greentea-client/test_env.h"
#include "mbed.h"
#include "sim5320_driver.h"
#include "sim5320_tests_utils.h"
#include "string.h"
#include "unity.h"
#include "utest.h"
#include <time.h>

using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;

static utest::v1::status_t lib_test_setup_handler(const size_t number_of_cases)
{
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    modem->init();
    int err = 0;
    err = any_error(err, modem->reset());
    // run device and connect to network
    err = any_error(err, modem->network_set_params(MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN, MBED_CONF_SIM5320_DRIVER_TEST_APN, MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME, MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD));
    err = any_error(err, modem->network_up());
    return unite_utest_status_with_err(greentea_test_setup_handler(number_of_cases), err);
}

static utest::v1::status_t lib_case_setup_handler(const Case *const source, const size_t index_of_case)
{
    return greentea_case_setup_handler(source, index_of_case);
}

static utest::v1::status_t lib_case_teardown_handler(const Case *const source, const size_t passed, const size_t failed, const failure_t failure)
{
    return greentea_case_teardown_handler(source, passed, failed, failure);
}

static void lib_test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    // stop modem
    modem->network_down();
    delete modem;
    return greentea_test_teardown_handler(passed, failed, failure);
}

void test_sync_time()
{
    int err;
    time_t t = 0;
    SIM5320TimeService *time_service = modem->get_time_service();

    err = time_service->sync_time();
    TEST_ASSERT_EQUAL(0, err);

    time_service->get_time(&t);
    TEST_ASSERT_EQUAL(0, err);

    TEST_ASSERT_NOT_EQUAL(0, t);
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, lib_case_setup_handler, test_fun, lib_case_teardown_handler, greentea_case_failure_continue_handler)
Case cases[] = {
    SIM5320Case(test_sync_time),
};
Specification specification(lib_test_setup_handler, cases, lib_test_teardown_handler);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);

    // host handshake
    // note: it should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(200, "default_auto");
    // run tests
    return !Harness::run(specification);
}
