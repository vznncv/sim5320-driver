/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example shows FTP usage.
 *
 * Requirements:
 *
 * - active SIM card with an internet access
 *
 * Note: to run the example, you should adjust APN settings.
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

/**
 * Settings.
 */
#define MODEM_TX_PIN PD_8
#define MODEM_RX_PIN PD_9
#define MODEM_SIM_PIN ""
#define MODEM_SIM_APN "internet.mts.ru"
#define MODEM_SIM_APN_USERNAME "mts"
#define MODEM_SIM_APN_PASSWORD "mts"
#define APP_LED LED2
/**
 * Test FTP server settings
 */
#define FTP_URL "ftp://ftp.yandex.ru"
#define FTP_DEMO_DIR "/debian"
#define FTP_DEMO_FILE "/debian/README"

#define APP_ERROR(err, message) MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), message)
static int _check_ret_code(int res, const char *expr)
{
    static char err_msg[128];
    if (res < 0) {
        snprintf(err_msg, 128, "Expression \"%s\" failed (error code: %i)", expr, res);
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, res), err_msg);
    }
    return res;
}
#define CHECK_RET_CODE(expr) _check_ret_code(expr, #expr)

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

/**
 * Main code
 */

static DigitalOut led(APP_LED);

int main()
{
    // create driver
    SIM5320 sim5320(MODEM_TX_PIN, MODEM_RX_PIN);
    char buf[256];

    // reset and initialize device
    printf("Initialize modem ...\n");
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    CellularContext *context = sim5320.get_context();

    // set credential
    if (strlen(MODEM_SIM_PIN) > 0) {
        CHECK_RET_CODE(sim5320.get_device()->set_pin(MODEM_SIM_PIN));
    }
    // set APN settings
    context->set_credentials(MODEM_SIM_APN, MODEM_SIM_APN_USERNAME, MODEM_SIM_APN_PASSWORD);
    // connect to network
    CHECK_RET_CODE(context->connect()); // note: by default operations is blocking
    printf("The device has connected to network\n");

    // 1. Connect to ftp folder
    SIM5320FTPClient *ftp_client = sim5320.get_ftp_client();
    printf("Connect to \"%s\" ...\n", FTP_URL);
    CHECK_RET_CODE(ftp_client->connect(FTP_URL));
    printf("Connected\n");

    // 2. Change default location
    CHECK_RET_CODE(ftp_client->set_cwd(FTP_DEMO_DIR));

    // 3. Show directory
    SIM5320FTPClient::dir_entry_list_t dir_entry_list;
    CHECK_RET_CODE(ftp_client->listdir(FTP_DEMO_DIR, &dir_entry_list));
    sprintf(buf, "list directory \"%s\"", FTP_DEMO_DIR);
    print_header(buf);
    SIM5320FTPClient::dir_entry_t *dir_entry_ptr = dir_entry_list.get_head();
    while (dir_entry_ptr != NULL) {
        printf("- %s (%s)\n", dir_entry_ptr->name, dir_entry_ptr->d_type == DT_DIR ? "DIR" : "FILE");
        dir_entry_ptr = dir_entry_ptr->next;
    }
    print_separator();

    // 4. read file
    sprintf(buf, "File \"%s\"", FTP_DEMO_FILE);
    print_header(buf);
    CHECK_RET_CODE(ftp_client->download(FTP_DEMO_FILE, stdout));
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(ftp_client->disconnect());
    CHECK_RET_CODE(context->disconnect());
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        ThisThread::sleep_for(500ms);
        led = !led;
    }
}
