/**
 * Location service test case.
 *
 * Test requirements:
 *
 * - active SIM card
 * - an aviable network
 * - gps satellites should be available for an antenna
 */
#include <chrono>
#include <math.h>
#include <string.h>

#include "mbed.h"
#include "mbed_chrono.h"

#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include "sim5320_driver.h"
#include "sim5320_tests_utils.h"
#include "sim5320_utils.h"

using mbed::chrono::milliseconds_u32;
using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;
static sim5320::SIM5320LocationService *location_service;

static constexpr milliseconds_u32 GPS_RETRY_INTERVAL_MS = 30s;

static utest::v1::status_t app_case_setup_handler(const Case *const source, const size_t index_of_case)
{
    int err = 0;
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    modem->init();
    err = any_error(err, modem->reset());
    // set network settings
    err = any_error(err, modem->network_set_params(MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN, MBED_CONF_SIM5320_DRIVER_TEST_APN, MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME, MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD));
    // run modem
    err = any_error(err, modem->request_to_start());
    // reset gps data and settings
    location_service = modem->get_location_service();
    err = any_error(err, location_service->gps_xtra_set(false));
    err = any_error(err, location_service->gps_set_accuracy(50));
    err = any_error(err, location_service->gps_clear_data());
    return greentea_case_setup_handler(source, index_of_case);
}

static utest::v1::status_t app_case_teardown_handler(const Case *const source, const size_t passed, const size_t failed, const failure_t failure)
{
    // stop modem and release resources
    modem->request_to_stop();
    delete modem;
    return greentea_case_teardown_handler(source, passed, failed, failure);
}

static int coord_clear(SIM5320LocationService::coord_t &coord)
{
    coord.time = 0;
    coord.altitude = 0;
    coord.longitude = 0;
    coord.latitude = 0;
    return 0;
}

#define COORD_CLEAR(coord) coord_clear(coord)

static time_t build_time(int year, int month, int mday, int hour, int minute, int second)
{
    struct tm timeinfo;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = mday;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    return mktime(&timeinfo);
}

static time_t BASE_TIME = build_time(2020, 5, 1, 0, 0, 0);

static int coord_verify(SIM5320LocationService::coord_t &coord, int line_no)
{
    int err = 0;

    if (coord.time == 0) {
        UNITY_TEST_FAIL(line_no, "Expected that time is filled, but it isn't");
        err = -2;
    } else {
        // check time (GPS week rollover issue)
        if (coord.time < BASE_TIME) {
            UNITY_TEST_FAIL(line_no, "Invalid time. It should be grater that 2020-05-01, but it isn't.");
            err = -3;
        }
    }
    if (coord.latitude == 0) {
        UNITY_TEST_FAIL(line_no, "Expected that latitude is filled, but it isn't");
        err = -4;
    }
    if (coord.longitude == 0) {
        UNITY_TEST_FAIL(line_no, "Expected that longitude is filled, but it isn't");
        err = -5;
    }

    return err;
}

#define COORD_VERIFY(coord) coord_verify(coord, __LINE__)

static int gps_wait_and_read_coord(SIM5320LocationService *location_service, SIM5320LocationService::coord_t *coord, milliseconds_u32 timeout, milliseconds_u32 polling_interval = 1s)
{
    Timer tm;
    bool has_coord;
    int err;
    milliseconds_u32 total_timeout = timeout + polling_interval;

    tm.start();
    do {
        ThisThread::sleep_for(polling_interval);
        COORD_CLEAR(*coord);
        has_coord = false;
        err = location_service->gps_read_coord(coord, has_coord);
        if (err) {
            TEST_FAIL_MESSAGE("SIM5320LocationService::gps_read_coord method has failed");
            return err;
        }
        if (has_coord) {
            break;
        }
    } while (tm.elapsed_time() <= total_timeout);

    if (!has_coord) {
        char buf[80];
        sprintf(buf, "Fail to get coordinates within %i milliseconds", timeout.count());
        TEST_FAIL_MESSAGE(buf);
        return -1;
    }
    return 0;
}

void test_gps_auto_standalone()
{
    SIM5320LocationService::coord_t coord;
    bool has_coord;
    int err;

    // first attempts
    COORD_CLEAR(coord);
    has_coord = false;
    err = location_service->gps_locate(&coord, has_coord, SIM5320LocationService::GPS_MODE_STANDALONE);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, has_coord);
    COORD_VERIFY(coord);

    ThisThread::sleep_for(GPS_RETRY_INTERVAL_MS);

    // second attempt
    COORD_CLEAR(coord);
    has_coord = false;
    err = location_service->gps_locate(&coord, has_coord, SIM5320LocationService::GPS_MODE_STANDALONE);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, has_coord);
    COORD_VERIFY(coord);
}

void test_gps_auto_agps()
{
    SIM5320LocationService::coord_t coord;
    bool has_coord;
    int err;

    // set agps server and configure network
    err = location_service->gps_set_agps_server("supl.google.com:7276", true);
    TEST_ASSERT_EQUAL(0, err);
    err = modem->network_up();
    TEST_ASSERT_EQUAL(0, err);

    // first attempts
    COORD_CLEAR(coord);
    has_coord = false;
    err = location_service->gps_locate(&coord, has_coord, SIM5320LocationService::GPS_MODE_UE_BASED);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, has_coord);
    COORD_VERIFY(coord);

    ThisThread::sleep_for(GPS_RETRY_INTERVAL_MS);

    // second attempt
    COORD_CLEAR(coord);
    has_coord = false;
    err = location_service->gps_locate(&coord, has_coord, SIM5320LocationService::GPS_MODE_UE_BASED);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, has_coord);
    COORD_VERIFY(coord);

    // stop network
    modem->network_down();
}

void test_gps_standalone_cold_hot()
{
    SIM5320LocationService::coord_t coord;
    int err;

    // cold startup
    err = location_service->gps_start(SIM5320LocationService::GPS_MODE_STANDALONE, SIM5320LocationService::GPS_STARTUP_MODE_COLD);
    TEST_ASSERT_EQUAL(0, err);
    err = gps_wait_and_read_coord(location_service, &coord, 120s);
    TEST_ASSERT_EQUAL(0, err);
    COORD_VERIFY(coord);
    err = location_service->gps_stop();
    TEST_ASSERT_EQUAL(0, err);

    ThisThread::sleep_for(GPS_RETRY_INTERVAL_MS);

    // hot startup
    err = location_service->gps_start(SIM5320LocationService::GPS_MODE_STANDALONE, SIM5320LocationService::GPS_STARTUP_MODE_HOT);
    TEST_ASSERT_EQUAL(0, err);
    err = gps_wait_and_read_coord(location_service, &coord, 20s);
    TEST_ASSERT_EQUAL(0, err);
    COORD_VERIFY(coord);
    err = location_service->gps_stop();
    TEST_ASSERT_EQUAL(0, err);
}

void test_gps_xtra()
{
    SIM5320LocationService::coord_t coord;
    int err;

    // configure and download xtra file
    err = modem->network_up();
    TEST_ASSERT_EQUAL(0, err);
    err = location_service->gps_xtra_set(true);
    TEST_ASSERT_EQUAL(0, err);
    err = location_service->gps_xtra_download();
    TEST_ASSERT_EQUAL(0, err);
    err = modem->network_down();
    TEST_ASSERT_EQUAL(0, err);

    // run gps
    err = location_service->gps_start();
    TEST_ASSERT_EQUAL(0, err);
    err = gps_wait_and_read_coord(location_service, &coord, 100s);
    TEST_ASSERT_EQUAL(0, err);
    COORD_VERIFY(coord);
    err = location_service->gps_stop();
    TEST_ASSERT_EQUAL(0, err);

    // disable xtra
    err = location_service->gps_xtra_set(false);
    TEST_ASSERT_EQUAL(0, err);
}

void test_apgs()
{
    SIM5320LocationService::coord_t coord;
    int err;

    // set agps server and configure network
    err = location_service->gps_set_agps_server("supl.google.com:7276", true);
    TEST_ASSERT_EQUAL(0, err);
    err = modem->network_up();
    TEST_ASSERT_EQUAL(0, err);

    // agps mode
    err = location_service->gps_start(SIM5320LocationService::GPS_MODE_UE_BASED);
    TEST_ASSERT_EQUAL(0, err);
    err = gps_wait_and_read_coord(location_service, &coord, 80s);
    TEST_ASSERT_EQUAL(0, err);
    COORD_VERIFY(coord);
    err = location_service->gps_stop();
    TEST_ASSERT_EQUAL(0, err);

    // stop network
    modem->network_down();
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, app_case_setup_handler, test_fun, app_case_teardown_handler, greentea_case_failure_continue_handler)
static Case cases[] = {
    SIM5320Case(test_gps_auto_standalone),
    SIM5320Case(test_gps_auto_agps),
    SIM5320Case(test_gps_standalone_cold_hot),
    // note: AT+CGPSXD may not return code that causes failure. TODO: check
    // SIM5320Case(test_gps_xtra),
    SIM5320Case(test_apgs),
};
static Specification specification(greentea_test_setup_handler, cases);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);
    validate_test_apn_settings();

    // host handshake
    // note: it should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(1200, "default_auto");
    // run tests
    return !Harness::run(specification);
}
