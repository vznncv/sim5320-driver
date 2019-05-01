/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows sms usage.
 *
 *  Requirements:
 *
 * - active SIM card
 *
 * Pin map:
 *
 * - PB_10 - UART TX (SIM5320E)
 * - PB_11 - UART RX (SIM5320E)
 *
 * Note: for this example you should manually set your subscriber number or it should be stored in the SIM memory.
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

// ===========================================================================================
// Define your subscriber number or set "CNUM" to extract it from SIM card memory it it's set.
// ===========================================================================================
#define SUBSCRIBER_NUMBER "CNUM"

#define CHECK_RET_CODE(expr)                                                                              \
    {                                                                                                     \
        int err = expr;                                                                                   \
        if (err < 0) {                                                                                    \
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), "Expression \"" #expr "\" failed"); \
        }                                                                                                 \
    }

DigitalOut led(LED2);

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

nsapi_error_t attach_to_network(SIM5320 *sim5320)
{
    nsapi_error_t err = 0;
    CellularNetwork::AttachStatus attach_status;
    CellularNetwork *cellular_network = sim5320->get_network();

    for (int i = 0; i < 30; i++) {
        err = cellular_network->set_attach();
        if (!err) {
            break;
        }
        wait_ms(1000);
    }
    if (err) {
        return err;
    }

    for (int i = 0; i < 30; i++) {
        cellular_network->get_attach(attach_status);
        if (attach_status == CellularNetwork::Attached) {
            return NSAPI_ERROR_OK;
        }
        wait_ms(1000);
    }
    return NSAPI_ERROR_TIMEOUT;
}

nsapi_error_t detach_from_network(SIM5320 *sim5320)
{
    CellularNetwork *cellular_network = sim5320->get_network();
    return cellular_network->detach();
}

struct sms_reader_t {
    CellularSMS *sms;
    // count of received sms.
    volatile int sms_count;

    // buffer for a sms message
    static const uint16_t buf_len = SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE;
    char buf[buf_len];
    // buffer of phone number
    static const uint16_t phone_num_len = SMS_MAX_PHONE_NUMBER_SIZE;
    char phone_num[phone_num_len];
    // timestamp buffer
    static const uint16_t timestamp_buf_len = SMS_MAX_TIME_STAMP_SIZE;
    char timestamp_buf[timestamp_buf_len];

    nsapi_size_or_error_t read_last_sms()
    {
        nsapi_size_or_error_t err;
        int buf_size;
        // read last sms
        err = sms->get_sms(buf, buf_len, phone_num, phone_num_len, timestamp_buf, timestamp_buf_len, &buf_size);
        if (err < -1) {
            return err;
        }
        // show sms data
        printf("SMS:\n");
        if (err >= 0) {
            printf("  phone number: %s\n", phone_num);
            printf("  timestamp: %s\n", timestamp_buf);
            printf("  sms text: %s\n", buf);
        } else {
            err = 0;
            printf("  <<no SMS was found>>\n");
        }
        return err;
    }

    void process_new_sms()
    {
        sms_count++;
        printf("New sms!!!\n");
        CHECK_RET_CODE(read_last_sms());
    }
};

int main()
{
    // create driver
    SIM5320 sim5320(PB_10, PB_11);
    // reset and initialize device
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());

    // start modem and attach to network
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());
    printf("Attach to network ...\n");
    CHECK_RET_CODE(attach_to_network(&sim5320));

    SIM5320CellularDevice *cellular_device = (SIM5320CellularDevice *)sim5320.get_device();

    // get subscriber number
    char subscriber_number[SIM5320CellularDevice::SUBSCRIBER_NUMBER_MAX_LEN];
    strcpy(subscriber_number, SUBSCRIBER_NUMBER);
    if (strcmp(subscriber_number, "CNUM") == 0) {
        CHECK_RET_CODE(cellular_device->get_subscriber_number(subscriber_number));
        if (strcmp(subscriber_number, "") == 0) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_INVALID_ARGUMENT), "Subscriber number isn't set explicitly and isn't found in the SIM card memory.");
        }
    }
    printf("Subscriber number: %s\n", subscriber_number);

    // SMS demo
    CellularSMS *sms = sim5320.get_sms();
    sms_reader_t sms_reader = { .sms = sms, .sms_count = 0 };
    char sms_message[128];
    uint32_t message_id = us_ticker_read() & 0xFF;
    sprintf(sms_message, "SIM5320 demo message (ID 0x%02X)", message_id);

    print_header("SMS demo");

    // 1. set SMS mode
    CHECK_RET_CODE(sms->initialize(CellularSMS::CellularSMSMmodeText));
    // 2. use device memory to store/read sms
    CHECK_RET_CODE(sms->set_cpms("ME", "ME", "ME"));
    // 3. read existed sms
    printf("## last sms\n");
    CHECK_RET_CODE(sms_reader.read_last_sms());
    // 4. clear sms messages
    printf("## clear sms messages\n");
    CHECK_RET_CODE(sms->delete_all_messages());
    // 5. SMS echo demo
    sms->set_sms_callback(callback(&sms_reader, &sms_reader_t::process_new_sms));
    printf("## Send echo sms \"%s\" to \"%s\"...\n", sms_message, subscriber_number);
    CHECK_RET_CODE(sms->send_sms(subscriber_number, sms_message, strlen(sms_message)));
    printf("## Wait sms ...\n");
    for (int i = 0; i < 60; i++) {
        sim5320.process_urc();
        if (sms_reader.sms_count > 0) {
            break;
        }
        wait_ms(1000);
    }
    if (sms_reader.sms_count == 0) {
        printf("Fail. SMS isn't received!!!\n");
    }
    print_separator();

    // stop modem
    printf("Detach from network ...\n");
    CHECK_RET_CODE(detach_from_network(&sim5320));
    printf("Stop ...\n");
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
