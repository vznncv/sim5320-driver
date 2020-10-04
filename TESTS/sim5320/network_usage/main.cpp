/**
 * Base network usage test case.
 *
 * The test requires active SIM card and an aviable network.
 */

#include <chrono>
#include <math.h>
#include <string.h>

#include "mbed.h"

#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include "sim5320_driver.h"
#include "sim5320_tests_utils.h"
#include "sim5320_utils.h"

using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;

static utest::v1::status_t test_setup_handler(const size_t number_of_cases)
{
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    modem->init();
    int err = 0;
    err = any_error(err, modem->reset());
    // set PIN if we have it
    const char *pin = MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN;
    if (strlen(pin) > 0) {
        modem->get_device()->set_pin(pin);
    }
    // connect to network
    err = any_error(err, modem->request_to_start());
    // connect to network
    CellularContext *cellular_context = modem->get_context();
    cellular_context->set_credentials(MBED_CONF_SIM5320_DRIVER_TEST_APN, MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME, MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD);
    err = any_error(err, cellular_context->connect());

    return unite_utest_status_with_err(greentea_test_setup_handler(number_of_cases), err);
}

static void test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    // detach from network
    CellularContext *cellular_context = modem->get_context();
    cellular_context->disconnect();
    // stop modem (CellularDevise::shutdown)
    modem->request_to_stop();
    delete modem;
    return greentea_test_teardown_handler(passed, failed, failure);
}

static utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    return greentea_case_setup_handler(source, index_of_case);
}

void test_dns_usage()
{
    int err;
    CellularDevice *cellular_device = modem->get_device();
    CellularContext *cellular_context = modem->get_context();

    // check dns usage
    SocketAddress address;
    const char *test_host = "example.com";
    err = cellular_context->gethostbyname(test_host, &address);
    TEST_ASSERT(address);
    TEST_ASSERT_NOT_NULL(address.get_ip_address());
}

void test_tcp_usage()
{
    int err;
    CellularContext *cellular_context = modem->get_context();

    // resolve host address
    const char *host = "example.com";
    const int port = 80;
    SocketAddress address;
    err = cellular_context->gethostbyname(host, &address);
    TEST_ASSERT_EQUAL(0, err);
    address.set_port(port);
    // prepare request
    char request[128];
    sprintf(request,
        "GET / HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n\r\n",
        host, port);

    // open socket
    TCPSocket socket;
    socket.set_timeout(2000);
    err = socket.open(cellular_context);
    TEST_ASSERT_EQUAL(0, err);
    err = socket.connect(address);
    TEST_ASSERT_EQUAL(0, err);

    // send request
    nsapi_size_t request_len = strlen(request);
    int sent_bytes = 0;
    while (sent_bytes < request_len) {
        nsapi_size_t n = socket.send(request + sent_bytes, request_len - sent_bytes);
        if (n < 0) {
            break;
        }
        sent_bytes += n;
    }
    TEST_ASSERT_EQUAL(request_len, sent_bytes);

    // read and check response
    const int response_buf_size = 128;
    char response_buf[response_buf_size];
    int read_bytes = 0;
    nsapi_size_or_error_t read_code;
    while ((read_code = socket.recv(response_buf, response_buf_size)) > 0) {
        read_bytes += read_code;
    }
    TEST_ASSERT_EQUAL(0, read_code);
    TEST_ASSERT(read_bytes > 0);

    // close socket
    err = socket.close();
    TEST_ASSERT_EQUAL(0, err);
}

void test_udp_usage()
{
    int err;
    CellularContext *cellular_context = modem->get_context();

    // resolve NTP server address
    SocketAddress ntp_address;
    err = cellular_context->gethostbyname("2.pool.ntp.org", &ntp_address);
    TEST_ASSERT_EQUAL(0, err);
    ntp_address.set_port(123);

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
    int recv_bytes_num;
    int sent_bytes_num;

    // open socket
    UDPSocket socket;
    err = socket.open(cellular_context);
    TEST_ASSERT_EQUAL(0, err);

    // send request
    sent_bytes_num = socket.sendto(ntp_address, ntr_request, ntp_request_size);
    TEST_ASSERT_EQUAL(ntp_request_size, sent_bytes_num);

    // read response
    recv_bytes_num = socket.recv(ntr_response, ntp_response_size);
    TEST_ASSERT(recv_bytes_num > 0);

    // close socket
    err = socket.close();
    TEST_ASSERT_EQUAL(0, err);

    // parse and check response
    TEST_ASSERT(recv_bytes_num == ntp_response_size);
    uint32_t seconds_since_1900 = 0;
    for (int i = 40; i < 44; i++) {
        seconds_since_1900 <<= 8;
        seconds_since_1900 += ntr_response[i];
    }
    time_t current_time = seconds_since_1900 - TIME1970;
    time_t time_2019 = 1546300800L;
    TEST_ASSERT(current_time > time_2019);
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
static Case cases[] = {
    SIM5320Case(test_dns_usage),
    SIM5320Case(test_tcp_usage),
    SIM5320Case(test_udp_usage),
};
static Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);
    validate_test_apn_settings();

    // host handshake
    // note: should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(80, "default_auto");
    // run tests
    return !Harness::run(specification);
}
