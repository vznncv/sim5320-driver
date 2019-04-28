/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * The example shows FTP usage.
 *
 * Requirements:
 *
 * - active SIM card with an internet access
 *
 * Pin map:
 *
 * - PB_10 - UART TX (SIM5320E)
 * - PB_11 - UART RX (SIM5320E)
 *
 * Note: to run the example, you should an adjust APN settings in the code.
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

#define APP_ERROR(err, message) MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), message)
#define CHECK_RET_CODE(expr)                                                           \
    {                                                                                  \
        int err = expr;                                                                \
        if (err < 0) {                                                                 \
            char err_msg[64];                                                          \
            sprintf(err_msg, "Expression \"" #expr "\" failed (error code: %i)", err); \
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err), err_msg);        \
        }                                                                              \
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

int main()
{
    // create driver
    SIM5320 sim5320(PB_10, PB_11);
    char buf[256];
    const char *ftp_path = "ftp://ftp.yandex.ru";
    const char *demo_dir = "/debian";
    const char *demo_file = "/debian/README";

    // reset and initialize device
    CHECK_RET_CODE(sim5320.reset());
    CHECK_RET_CODE(sim5320.init());
    printf("Start ...\n");
    CHECK_RET_CODE(sim5320.request_to_start());

    CellularContext *context = sim5320.get_context();
    // set credential
    //context->set_sim_pin("1234");
    // note: set your APN parameters
    context->set_credentials("internet.mts.ru", "mts", "mts");
    // connect to network
    CHECK_RET_CODE(context->connect()); // note: by default operations is blocking
    printf("The device has connected to network\n");

    // 1. Connect to ftp folder
    SIM5320FTPClient *ftp_client = sim5320.get_ftp_client();
    printf("Connect to \"%s\" ...\n", ftp_path);
    CHECK_RET_CODE(ftp_client->connect(ftp_path));
    printf("Connected\n");

    // 2. Change default location
    CHECK_RET_CODE(ftp_client->set_cwd(demo_dir));

    // 3. Show directory
    SIM5320FTPClient::dir_entry_list_t dir_entry_list;
    CHECK_RET_CODE(ftp_client->listdir(demo_dir, &dir_entry_list));
    sprintf(buf, "list directory \"%s\"", demo_dir);
    print_header(buf);
    SIM5320FTPClient::dir_entry_t *dir_entry_ptr = dir_entry_list.get_head();
    while (dir_entry_ptr != NULL) {
        printf("- %s (%s)\n", dir_entry_ptr->name, dir_entry_ptr->d_type == DT_DIR ? "DIR" : "FILE");
        dir_entry_ptr = dir_entry_ptr->next;
    }
    print_separator();

    // 4. read file
    sprintf(buf, "File \"%s\"", demo_file);
    print_header(buf);
    CHECK_RET_CODE(ftp_client->download(demo_file, stdout));
    print_separator();

    printf("Stop ...\n");
    CHECK_RET_CODE(ftp_client->disconnect());
    CHECK_RET_CODE(context->disconnect());
    CHECK_RET_CODE(sim5320.request_to_stop());
    printf("Complete!\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
