/**
 * Base test to check that AT interface works.
 *
 * It doesn't require any SIM card.
 */

#include "greentea-client/test_env.h"
#include "math.h"
#include "mbed.h"
#include "rtos.h"
#include "sim5320_driver.h"
#include "sim5320_tests_utils.h"
#include "string.h"
#include "unity.h"
#include "utest.h"

using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;

utest::v1::status_t test_setup_handler(const size_t number_of_cases)
{
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    int err = modem->reset();
    return unite_utest_status_with_err(greentea_test_setup_handler(number_of_cases), err);
}

void test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    delete modem;
    return greentea_test_teardown_handler(passed, failed, failure);
}

utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    // reset modem
    int err = modem->init();
    return unite_utest_status_with_err(greentea_case_setup_handler(source, index_of_case), err);
}

void test_software_reset()
{
    int err = modem->reset(SIM5320::RESET_MODE_SOFT);
    TEST_ASSERT_EQUAL(0, err);
}

void test_hardware_reset()
{
    if (MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN != NC) {
        int err = modem->reset(SIM5320::RESET_MODE_HARD);
        TEST_ASSERT_EQUAL(0, err);
    } else {
        TEST_IGNORE_MESSAGE("sim5320-driver.test_reset_pin isn't set. Skip test");
    }
}

/**
 * Test modem initialization.
 */
void test_init_state()
{
    // check that init isn't failed
    int err = modem->init();
    TEST_ASSERT_EQUAL(0, err);
}

void test_cellular_info_manufacturer()
{
    const size_t buf_size = 128;
    char buf[buf_size];
    buf[0] = '\0';

    int err = modem->get_information()->get_manufacturer(buf, buf_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(has_substring(buf, "SIMCOM"));
}

void test_cellular_info_model()
{
    const size_t buf_size = 128;
    char buf[buf_size];
    buf[0] = '\0';

    int err = modem->get_information()->get_model(buf, buf_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(has_substring(buf, "SIM5320"));
}

void test_cellular_info_revision()
{
    const size_t buf_size = 128;
    char buf[buf_size];
    buf[0] = '\0';

    int err = modem->get_information()->get_revision(buf, buf_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(not_empty(buf));
}

void test_cellular_info_serial_number_sn()
{
    const size_t buf_size = 128;
    char buf[buf_size];
    buf[0] = '\0';

    // test serial numbers
    int err = modem->get_information()->get_serial_number(buf, buf_size, CellularInformation::SN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(not_empty(buf));
}

void test_cellular_info_serial_number_imei()
{
    const size_t buf_size = 128;
    char buf[buf_size];
    buf[0] = '\0';

    // test serial numbers
    int err = modem->get_information()->get_serial_number(buf, buf_size, CellularInformation::IMEI);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(not_empty(buf));
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
Case cases[] = {
    SIM5320Case(test_software_reset),
    SIM5320Case(test_hardware_reset),
    SIM5320Case(test_init_state),
    SIM5320Case(test_cellular_info_manufacturer),
    SIM5320Case(test_cellular_info_model),
    SIM5320Case(test_cellular_info_revision),
    SIM5320Case(test_cellular_info_serial_number_sn),
    SIM5320Case(test_cellular_info_serial_number_imei),
};
Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);

    // host handshake
    // note: should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(80, "default_auto");
    // run tests
    return !Harness::run(specification);
}
