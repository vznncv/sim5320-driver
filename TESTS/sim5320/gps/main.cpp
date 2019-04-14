/**
 * GPS test case.
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

static int any_error(int err_1, int err_2)
{
    if (err_1) {
        return err_1;
        ;
    }
    return err_2;
}

static sim5320::SIM5320 *modem;

utest::v1::status_t test_setup_handler(const size_t number_of_cases)
{
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    int err = 0;
    err = any_error(err, modem->reset());
    // set PIN if we have it
    const char *pin = MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN;
    if (strlen(pin) > 0) {
        modem->get_device()->set_pin(pin);
    }
    // run modem
    err = any_error(err, modem->request_to_start());

    status_t res = greentea_test_setup_handler(number_of_cases);
    return err ? STATUS_ABORT : res;
}

void test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    // stop modem (CellularDevise::shutdown)
    modem->request_to_stop();
    delete modem;

    return greentea_test_teardown_handler(passed, failed, failure);
}

utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    // ensure that GPS is disabled
    modem->get_gps()->stop();
    return greentea_case_setup_handler(source, index_of_case);
}

void test_gps_usage()
{
    int err;
    bool gps_is_active;
    bool has_coordinates;
    SIM5320GPSDevice::gps_coord_t gps_coord;
    SIM5320GPSDevice *gps = modem->get_gps();

    // run gps
    err = gps->start();
    TEST_ASSERT_EQUAL(0, err);

    // set accuracy
    const int desired_accuracy = 30;
    int actual_ccuracy;
    err = gps->set_desired_accuracy(desired_accuracy);
    TEST_ASSERT_EQUAL(0, err);
    err = gps->get_desired_accuracy(actual_ccuracy);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(desired_accuracy, actual_ccuracy);

    // check gps status
    err = gps->is_active(gps_is_active);
    TEST_ASSERT_EQUAL(true, gps_is_active);

    SIM5320GPSDevice::Mode gps_mode;
    err = gps->get_mode(gps_mode);
    TEST_ASSERT_EQUAL(0, err);

    // check that get_coord isn't failed
    for (int i = 0; i < 5; i++) {
        err = gps->get_coord(has_coordinates, gps_coord);
        TEST_ASSERT_EQUAL(0, err);

        wait_ms(1000);
    }

    // stop gps
    err = gps->stop();
    TEST_ASSERT_EQUAL(0, err);
    // check status
    err = gps->is_active(gps_is_active);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, gps_is_active);
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
Case cases[] = {
    SIM5320Case(test_gps_usage)
};
Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // host handshake
    // note: it should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(100, "default_auto");
    // run tests
    return !Harness::run(specification);
}
