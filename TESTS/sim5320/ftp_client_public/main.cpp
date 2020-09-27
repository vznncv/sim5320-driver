/**
 * FTP client test case.
 *
 * The test requires:
 * - active SIM card
 * - an aviable network.
 *
 *  note: it uses public FTP server, so you don't need your own server, but it doesn't test whole functionality
 */

#include <math.h>
#include <string.h>

#include "LittleFileSystem.h"
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
static FileSystem *fs;
static BlockDevice *block_device;

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
    // run modem
    err = any_error(err, modem->request_to_start());
    // connect to network
    CellularContext *cellular_context = modem->get_context();
    cellular_context->set_credentials(MBED_CONF_SIM5320_DRIVER_TEST_APN, MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME, MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD);
    err = any_error(err, cellular_context->connect());

    // create test file system
    block_device = new HeapBlockDevice(4096, 128);
    fs = new LittleFileSystem("heap", block_device);

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

    // delete file system
    fs->unmount();
    delete fs;
    delete block_device;

    return greentea_test_teardown_handler(passed, failed, failure);
}

static utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    // clear file system
    fs->reformat(block_device);
    return greentea_case_setup_handler(source, index_of_case);
}

// Test connection to FTP server
void test_ftp_connect()
{
    int err;
    SIM5320FTPClient *ftp_client = modem->get_ftp_client();

    // connect
    err = ftp_client->connect(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTP_URL);
    TEST_ASSERT_EQUAL(0, err);

    // send CWD command to check if connection is valid
    err = ftp_client->set_cwd("/");
    TEST_ASSERT_EQUAL(0, err);

    // disconnect
    err = ftp_client->disconnect();
    TEST_ASSERT_EQUAL(0, err);
}

// Test connection to FTPS server with explicit SSL/TLS
void test_ftps_explicit_connect()
{
    int err;
    SIM5320FTPClient *ftp_client = modem->get_ftp_client();

    // connect
    err = ftp_client->connect(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTPS_EXPLICIT_URL);
    TEST_ASSERT_EQUAL(0, err);

    // send CWD command to check if connection is valid
    err = ftp_client->set_cwd("/");
    TEST_ASSERT_EQUAL(0, err);

    // disconnect
    err = ftp_client->disconnect();
    TEST_ASSERT_EQUAL(0, err);
}

// Test connection to FTPS server with implicit SSL/TLS
void test_ftps_implicit_connect()
{
    int err;
    SIM5320FTPClient *ftp_client = modem->get_ftp_client();

    // connect
    err = ftp_client->connect(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTPS_IMPLICIT_URL);
    TEST_ASSERT_EQUAL(0, err);

    // send CWD command to check if connection is valid
    err = ftp_client->set_cwd("/");
    TEST_ASSERT_EQUAL(0, err);

    // disconnect
    err = ftp_client->disconnect();
    TEST_ASSERT_EQUAL(0, err);
}

void test_common()
{
    int err;
    char path_buf[128];
    char current_dir_buf[128];
    const char *local_file_path = "/heap/text.data";
    SIM5320FTPClient::dir_entry_list_t dir_entry_list;
    SIM5320FTPClient::dir_entry_t *dir_entry_ptr;
    long file_size;
    bool check_res;
    size_t MAX_FILE_SIZE_TO_READ = 1536;
    SIM5320FTPClient *ftp_client = modem->get_ftp_client();

    // connect to server and prepare test directory
    err = ftp_client->connect(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTPS_EXPLICIT_URL);
    TEST_ASSERT_EQUAL(0, err);

    // check root content and try to read files if they exists
    err = ftp_client->get_cwd(current_dir_buf, 128);
    TEST_ASSERT_EQUAL(0, err);
    err = ftp_client->listdir(current_dir_buf, &dir_entry_list);
    TEST_ASSERT_EQUAL(0, err);
    if (!err) {
        for (dir_entry_ptr = dir_entry_list.get_head(); dir_entry_ptr != NULL; dir_entry_ptr = dir_entry_ptr->next) {
            sprintf(path_buf, "%s/%s", current_dir_buf, dir_entry_ptr->name);
            if (dir_entry_ptr->d_type == DT_DIR) {
                // it's directory
                err = ftp_client->isdir(path_buf, check_res);
                TEST_ASSERT_EQUAL(0, err);
                TEST_ASSERT_EQUAL(true, check_res);
                err = ftp_client->isfile(path_buf, check_res);
                TEST_ASSERT_EQUAL(0, err);
                TEST_ASSERT_EQUAL(false, check_res);
                err = ftp_client->exists(path_buf, check_res);
                TEST_ASSERT_EQUAL(0, err);
                TEST_ASSERT_EQUAL(true, check_res);

            } else {
                // it's file
                err = ftp_client->isdir(path_buf, check_res);
                TEST_ASSERT_EQUAL(0, err);
                TEST_ASSERT_EQUAL(false, check_res);
                err = ftp_client->isfile(path_buf, check_res);
                TEST_ASSERT_EQUAL(0, err);
                TEST_ASSERT_EQUAL(true, check_res);
                err = ftp_client->exists(path_buf, check_res);
                TEST_ASSERT_EQUAL(0, err);
                TEST_ASSERT_EQUAL(true, check_res);

                // extra test for a file
                err = ftp_client->get_file_size(path_buf, file_size);
                TEST_ASSERT_EQUAL(0, err);
                if (!err && file_size <= MAX_FILE_SIZE_TO_READ) {
                    err = ftp_client->download(path_buf, local_file_path);
                    TEST_ASSERT_EQUAL(0, err);

                    // check file
                    struct stat file_stat;
                    err = stat(local_file_path, &file_stat);
                    TEST_ASSERT_EQUAL(0, err);
                    TEST_ASSERT_EQUAL(S_IFREG, file_stat.st_mode & S_IFMT);
                    TEST_ASSERT_EQUAL(file_size, file_stat.st_size);
                    // remove file
                    remove(local_file_path);
                }
            }
        }
    }
    // check that current directory isn't changed
    err = ftp_client->get_cwd(path_buf, 128);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(current_dir_buf, path_buf);

    // disconnect
    err = ftp_client->disconnect();
    TEST_ASSERT_EQUAL(0, err);
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
static Case cases[] = {
    SIM5320Case(test_ftp_connect),
    SIM5320Case(test_ftps_explicit_connect),
    SIM5320Case(test_ftps_implicit_connect),
    SIM5320Case(test_common),
};
static Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);
    validate_test_ftp_settings(true, false);

    // host handshake
    // note: it should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(200, "default_auto");
    // run tests
    return !Harness::run(specification);
}
