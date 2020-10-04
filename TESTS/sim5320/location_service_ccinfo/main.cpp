/**
 * Time service client test case.
 *
 * The test requires:
 * - active SIM card
 * - an aviable network.
 */
#include <string.h>
#include <time.h>

#include "mbed.h"

#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include "sim5320_driver.h"
#include "sim5320_tests_utils.h"

using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;

static utest::v1::status_t lib_test_setup_handler(const size_t number_of_cases)
{
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    modem->init();
    // set network parameters
    int err = modem->network_set_params(MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN, MBED_CONF_SIM5320_DRIVER_TEST_APN, MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME, MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD);
    return unite_utest_status_with_err(greentea_test_setup_handler(number_of_cases), err);
}

static utest::v1::status_t lib_case_setup_handler(const Case *const source, const size_t index_of_case)
{
    int err = modem->reset();
    ThisThread::sleep_for(1000ms);
    return unite_utest_status_with_err(greentea_case_setup_handler(source, index_of_case), err);
}

static utest::v1::status_t lib_case_teardown_handler(const Case *const source, const size_t passed, const size_t failed, const failure_t failure)
{
    return greentea_case_teardown_handler(source, passed, failed, failure);
}

static void lib_test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    delete modem;
    return greentea_test_teardown_handler(passed, failed, failure);
}

void test_ccinfo_device_after_start()
{
    SIM5320LocationService::station_info_t station_info;
    int err;
    bool has_data;
    SIM5320LocationService *location_service = modem->get_location_service();

    err = location_service->cell_system_read_info(&station_info, has_data);
    TEST_ASSERT_EQUAL(0, err);

    TEST_ASSERT_FALSE(has_data)
}

void test_ccinfo_after_device_up()
{
    SIM5320LocationService::station_info_t station_info;
    int err;
    bool has_data;

    err = modem->request_to_start();
    TEST_ASSERT_EQUAL(0, err);

    SIM5320LocationService *location_service = modem->get_location_service();

    err = location_service->cell_system_read_info(&station_info, has_data);
    TEST_ASSERT_EQUAL(0, err);

    // check that data isn't available after device startup immediaty
    TEST_ASSERT_FALSE(has_data)

    err = modem->request_to_stop();
    TEST_ASSERT_EQUAL(0, err);
}

void test_ccinfo_device_up()
{
    SIM5320LocationService::station_info_t station_info;
    int err;
    bool has_data;

    err = modem->request_to_start();
    TEST_ASSERT_EQUAL(0, err);

    ThisThread::sleep_for(10000ms);
    SIM5320LocationService *location_service = modem->get_location_service();

    err = location_service->cell_system_read_info(&station_info, has_data);
    TEST_ASSERT_EQUAL(0, err);

    // check that we have found some station
    TEST_ASSERT_TRUE(has_data);
    TEST_ASSERT_NOT_EQUAL(0, station_info.mnc);
    TEST_ASSERT_NOT_EQUAL(0, station_info.mcc);
    TEST_ASSERT_NOT_EQUAL(0, station_info.lac);
    TEST_ASSERT_NOT_EQUAL(0, station_info.cid);
    TEST_ASSERT(station_info.signal_db < 0);
    TEST_ASSERT(0 <= station_info.network_type && station_info.network_type < 2);

    err = modem->request_to_stop();
    TEST_ASSERT_EQUAL(0, err);
}

void test_ccinfo_device_network_up()
{
    SIM5320LocationService::station_info_t station_info;
    int err;
    bool has_data;

    err = modem->network_up();
    TEST_ASSERT_EQUAL(0, err);
    if (err) {
        return;
    }

    SIM5320LocationService *location_service = modem->get_location_service();

    err = location_service->cell_system_read_info(&station_info, has_data);
    TEST_ASSERT_EQUAL(0, err);

    // check that we have found some station
    TEST_ASSERT_TRUE(has_data);
    TEST_ASSERT_NOT_EQUAL(0, station_info.mnc);
    TEST_ASSERT_NOT_EQUAL(0, station_info.mcc);
    TEST_ASSERT_NOT_EQUAL(0, station_info.lac);
    TEST_ASSERT_NOT_EQUAL(0, station_info.cid);
    TEST_ASSERT(station_info.signal_db < 0);
    TEST_ASSERT(0 <= station_info.network_type && station_info.network_type < 2);

    err = modem->network_down();
    TEST_ASSERT_EQUAL(0, err);
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, lib_case_setup_handler, test_fun, lib_case_teardown_handler, greentea_case_failure_continue_handler)
static Case cases[] = {
    SIM5320Case(test_ccinfo_device_after_start),
    SIM5320Case(test_ccinfo_after_device_up),
    SIM5320Case(test_ccinfo_device_up),
    SIM5320Case(test_ccinfo_device_network_up),
};
static Specification specification(lib_test_setup_handler, cases, lib_test_teardown_handler);

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
