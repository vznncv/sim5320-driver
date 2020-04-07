/**
 * SMS API test.
 *
 * The test requires:
 * - active SIM card
 * - an aviable network
 * - SIM subscriber number that is set using test_sim_subscriber_number, or is located in the SIM card memory (see CNUM command)
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

#if not MBED_CONF_CELLULAR_USE_SMS
#error "To run SMS test, the cellular.use-sms option must be enabled"
#endif // MBED_CONF_CELLULAR_USE_SMS

using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;
static CellularSMS *sms;
static char subscriber_number[SIM5320CellularDevice::SUBSCRIBER_NUMBER_MAX_LEN];

static nsapi_error_t attach_to_network(SIM5320 *sim5320)
{
    nsapi_error_t err = 0;
    CellularNetwork::AttachStatus attach_status;
    CellularNetwork *cellular_network = sim5320->get_network();

    for (int i = 0; i < 30; i++) {
        err = cellular_network->set_attach();
        if (!err) {
            break;
        }
        ThisThread::sleep_for(1000);
    }
    if (err) {
        return err;
    }

    for (int i = 0; i < 30; i++) {
        cellular_network->get_attach(attach_status);
        if (attach_status == CellularNetwork::Attached) {
            return NSAPI_ERROR_OK;
        }
        ThisThread::sleep_for(1000);
    }
    return NSAPI_ERROR_TIMEOUT;
}

static nsapi_error_t detach_from_network(SIM5320 *sim5320)
{
    CellularNetwork *cellular_network = sim5320->get_network();
    return cellular_network->detach();
}

#define ABORT_TEST_SETUP_HANDLER_IF_ERROR(expr)           \
    do {                                                  \
        int err = expr;                                   \
        if (err) {                                        \
            greentea_test_setup_handler(number_of_cases); \
            return STATUS_ABORT;                          \
        }                                                 \
    } while (0);

utest::v1::status_t test_setup_handler(const size_t number_of_cases)
{
    // initialize modem
    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    modem->init();
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(modem->reset());
    ThisThread::sleep_for(500);
    // set PIN if we have it
    const char *pin = MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN;
    if (strlen(pin) > 0) {
        modem->get_device()->set_pin(pin);
    }
    // run modem
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(modem->request_to_start());

    // resolve subscriber number
    if (strcmp(MBED_CONF_SIM5320_DRIVER_TEST_SIM_SUBSCRIBER_NUMBER, "CNUM") == 0) {
        SIM5320CellularDevice *cellular_device = (SIM5320CellularDevice *)modem->get_device();
        ABORT_TEST_SETUP_HANDLER_IF_ERROR(cellular_device->get_subscriber_number(subscriber_number));
    }

    // connect to network
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(attach_to_network(modem));

    // initialize SMS device
    sms = modem->get_sms();
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(sms->initialize(CellularSMS::CellularSMSMmodeText));

    return greentea_test_setup_handler(number_of_cases);
}

void test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    // disconnect from network
    detach_from_network(modem);
    // stop modem (CellularDevise::shutdown)
    modem->request_to_stop();
    // cleanup resources
    delete modem;

    return greentea_test_teardown_handler(passed, failed, failure);
}

utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    return greentea_case_setup_handler(source, index_of_case);
}

void test_get_sms()
{
    // buffer for a sms message
    static const uint16_t buf_len = SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE;
    char buf[buf_len] = "";
    // buffer of phone number
    static const uint16_t phone_num_len = SMS_MAX_PHONE_NUMBER_SIZE;
    char phone_num[phone_num_len] = "";
    // timestamp buffer
    static const uint16_t timestamp_buf_len = SMS_MAX_TIME_STAMP_SIZE;
    char timestamp_buf[timestamp_buf_len] = "";

    nsapi_size_or_error_t err;
    int buf_size;
    // read last sms
    err = sms->get_sms(buf, buf_len, phone_num, phone_num_len, timestamp_buf, timestamp_buf_len, &buf_size);

    if (err == -1) {
        // sms not found
    } else if (err < -1) {
        // error
        TEST_FAIL_MESSAGE("get_sms method returned error");
    } else {
        // check sms description
        TEST_ASSERT_MESSAGE(strlen(buf) > 0, "buf if empty");
        TEST_ASSERT_MESSAGE(strlen(phone_num) > 0, "phone_num if empty");
        TEST_ASSERT_MESSAGE(strlen(timestamp_buf) > 0, "timestamp_buf if empty");
    }
}

static uint8_t generate_message_id()
{
    return us_ticker_read() & 0xFF;
}

struct sms_reader_t {
    CellularSMS *sms;
    // count of received sms.
    int sms_count;
    int last_err;

    // buffer for a sms message
    static const uint16_t buf_len = SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE;
    char buf[buf_len];
    // buffer of phone number
    static const uint16_t phone_num_len = SMS_MAX_PHONE_NUMBER_SIZE;
    char phone_num[phone_num_len];
    // timestamp buffer
    static const uint16_t timestamp_buf_len = SMS_MAX_TIME_STAMP_SIZE;
    char timestamp_buf[timestamp_buf_len];

    sms_reader_t(CellularSMS *sms)
        : sms(sms)
    {
    }

    void clear()
    {
        strcpy(buf, "");
        strcpy(phone_num, "");
        strcpy(timestamp_buf, "");
        sms_count = 0;
        last_err = 0;
    }

    nsapi_error_t read_last_sms()
    {
        nsapi_size_or_error_t err;
        int buf_size;
        // read last sms
        err = sms->get_sms(buf, buf_len, phone_num, phone_num_len, timestamp_buf, timestamp_buf_len, &buf_size);
        if (err >= 0) {
            err = 0;
        }
        return err;
    }

    void process_new_sms()
    {
        sms_count++;
        last_err = read_last_sms();
    }
};

void test_sms_workflow()
{
    nsapi_size_or_error_t err;
    sms_reader_t sms_reader(sms);
    sms_reader.clear();

    // 1. delete all existed sms
    TEST_ASSERT_EQUAL(0, sms->delete_all_messages());
    // 2. check that there are no sms messages
    err = sms_reader.read_last_sms();
    if (err != -1) {
        TEST_FAIL();
        return;
    }
    // 3. create test message
    char sms_message[32];
    sprintf(sms_message, "Test message. ID: 0x%02X", generate_message_id());

    // 4. attach sms handler
    sms->set_sms_callback(callback(&sms_reader, &sms_reader_t::process_new_sms));
    // 5. send sms to myself
    err = sms->send_sms(subscriber_number, sms_message, strlen(sms_message));
    if (err < 0) {
        TEST_FAIL_MESSAGE("Fail to send sms");
        return;
    }
    // 6. wait new sms
    for (int i = 0; i < 30; i++) {
        modem->process_urc();
        if (sms_reader.sms_count > 0) {
            break;
        }
        ThisThread::sleep_for(1000);
    }

    // 7. check sms
    if (sms_reader.sms_count == 0) {
        TEST_FAIL_MESSAGE("sms isn't received");
        return;
    } else {
        TEST_ASSERT_EQUAL(1, sms_reader.sms_count);
        TEST_ASSERT_EQUAL_STRING(sms_message, sms_reader.buf);
        TEST_ASSERT_EQUAL_STRING(subscriber_number, sms_reader.phone_num);
        TEST_ASSERT(strlen(sms_reader.timestamp_buf) > 0);
    }
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
Case cases[] = {
    SIM5320Case(test_get_sms),
    SIM5320Case(test_sms_workflow)
};
Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);

    // host handshake
    // note: it should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(80, "default_auto");
    // run tests
    return !Harness::run(specification);
}
