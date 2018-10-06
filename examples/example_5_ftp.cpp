/**
 * Example of the SIM5320E usage with STM32F3Discovery board.
 *
 * This example shows FTPS usage.
 *
 * Pin map:
 *
 * - PC_4 - UART TX (stdout/stderr)
 * - PC_5 - UART RX (stdin)
 * - PB_10 - UART TX (SIM5320E)
 * - PB_11 - UART RX (SIM5320E)
 * - PB_14 - UART RTS (SIM5320E)
 * - PB_13 - UART CTS (SIM5320E)
 * - PB_12 - RST (Reset) pin (SIM5320E)
 */

#include "mbed.h"
#include <math.h>
#include <time.h>

#include "sim5320_driver.h"

using namespace sim5320;

DigitalOut led(LED2);

void print_separator()
{
    printf("====================================================================================\n");
}

void print_time(time_t* time)
{
    tm parsed_time;
    char str_buf[32];
    gmtime_r(time, &parsed_time);
    strftime(str_buf, 32, "%Y/%m/%d %H:%M:%S (UTC)", &parsed_time);
    printf("%s", str_buf);
}

void check_ret_code(int err_code, const char* action_name = NULL)
{
    if (err_code == 0) {
        return;
    }
    char error_message[80];
    if (action_name) {
        sprintf(error_message, "\"%s\" failed with code %d", action_name, err_code);
    } else {
        strcpy(error_message, "Device error");
    }
    print_separator();
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, err_code), error_message);
}

class DataGenerator {
private:
    size_t len;
    char sym;
    int sent_bytes_num;
    int progress_count;
    static const int PROGRESS_STEP = 1024;

public:
    DataGenerator(size_t len, char sym)
        : len(len)
        , sym(sym)
        , sent_bytes_num(0)
        , progress_count(0)
    {
    }

    ssize_t iterate(uint8_t* buf, size_t buf_len)
    {
        int data_to_write = len - sent_bytes_num;
        if (data_to_write > buf_len) {
            data_to_write = buf_len;
        }

        for (size_t i = 0; i < data_to_write; i++) {
            buf[i] = sym;
        }
        sent_bytes_num += data_to_write;
        progress_count += data_to_write;
        while (progress_count > PROGRESS_STEP) {
            progress_count -= PROGRESS_STEP;
            printf("progress: %8d/%d bytes\n", sent_bytes_num, len);
        }
        if (data_to_write == 0 && progress_count > 0) {
            printf("progress: %8d/%d bytes\n", sent_bytes_num, len);
        }

        return data_to_write;
    }
};

// simple led demo
int main()
{
    // create driver
    SIM5320 sim5320(PB_10, PB_11, PB_14, PB_13, PB_12);
    // reset device (we do it explicitly, as it isn't reset along with microcontroller)
    check_ret_code(sim5320.reset(), "device resetting");
    // perform basic driver initialization
    check_ret_code(sim5320.init(), "initialization");
    // check_ret_code(sim5320.start_uart_hw_flow_ctrl(), "start hardware control");
    printf("Start device ...\n");
    check_ret_code(sim5320.request_to_start(), "start full functionality mode");
    printf("The device has been run\n");

    // wait network registration
    printf("Connect to network ...\n");
    check_ret_code(sim5320.wait_network_registration(), "network registration");
    check_ret_code(sim5320.get_network()->set_credentials("internet.mts.ru", "mts", "mts"), "set credentials");
    check_ret_code(sim5320.get_network()->connect(), "connect");
    print_separator();

    // FTPS demo
    // connect to server
    SIM5320FTPClient* ftp = sim5320.get_ftp_client();
    const char* ftp_host = "someftp-server.com";
    const char* ftp_user = "user";
    const char* ftp_password = "password";
    SIM5320FTPClient::FTPProtocol ftp_protocol = SIM5320FTPClient::FTPS_TLS;
    int ftp_port = 22;

    printf("Connect to ftps server:\n");
    printf("  - host: %s\n", ftp_host);
    printf("  - port: %d\n", ftp_port);
    printf("  - user: %s\n", ftp_user);
    printf("  - pswd: %s\n", ftp_password);
    check_ret_code(ftp->connect(ftp_host, ftp_port, ftp_protocol, ftp_user, ftp_password), "connect to ftp");
    printf("Connected\n");

    // check that destination folder exists
    const char* file_dir = "/files";
    const char* file_path = "/files/sim5320_test.txt";
    bool dir_exists;
    check_ret_code(ftp->exists(file_dir, dir_exists), "check ftp dir");
    if (!dir_exists) {
        printf("create directory: %s\n", file_dir);
        check_ret_code(ftp->mkdir(file_dir), "create ftp dir");
    } else {
        printf("directory \"%s\" exists\n", file_dir);
    }

    // upload test file
    printf("Upload file \"%s\" ...\n", file_path);
    DataGenerator data_generator(1024 * 1024, 'C');
    check_ret_code(ftp->put(file_path, callback(&data_generator, &DataGenerator::iterate)), "ftp put");
    printf("The file has been uploaded.\n");

    printf("Disconnect from ftps server ...\n");
    check_ret_code(ftp->disconnect(), "disconnect from ftp");
    printf("Disconnected\n");

    print_separator();
    printf("Stop device ...\n");
    check_ret_code(sim5320.get_network()->disconnect(), "disconnect");
    check_ret_code(sim5320.request_to_stop(), "device shutdown");
    printf("Device has been stopped\n");

    while (1) {
        wait(0.5);
        led = !led;
    }
}
