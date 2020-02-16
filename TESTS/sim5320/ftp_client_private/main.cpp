/**
 * FTP client test case.
 *
 * The test requires:
 * - active SIM card
 * - an aviable network
 * - your server with private write permissions
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

#include "LittleFileSystem.h"

using namespace utest::v1;
using namespace sim5320;

static sim5320::SIM5320 *modem;
static FileSystem *fs;
static BlockDevice *block_device;
static sim5320::SIM5320FTPClient *ftp_client;
static const char *test_dir = MBED_CONF_SIM5320_DRIVER_TEST_FTP_READ_WRITE_OPERATIONS_DIR;

/**
 * Prepare test directory on the ftp server.
 *
 * After preparation we will get empty directory for tests.
 *
 * @param ftp_client
 * @return
 */
static int prepare_test_dir(SIM5320FTPClient *ftp_client, const char *test_dir)
{
    int err;

    // check if directory exists
    err = ftp_client->set_cwd(test_dir);
    if (err) {
        char test_dir_tmp[64];
        strcpy(test_dir_tmp, test_dir);
        // directory doesn't exists, create it
        for (char *dirname_end = test_dir_tmp + 1; *dirname_end != '\0'; dirname_end++) {
            if (*dirname_end == '/') {
                *dirname_end = '\0';
                ftp_client->mkdir(test_dir_tmp);
                *dirname_end = '/';
            }
        }
        err = ftp_client->mkdir(test_dir);
    } else {
        // directory exists, clear it
        err = ftp_client->rmtree(test_dir, false);
    }

    if (!err) {
        err = ftp_client->set_cwd("/");
    }

    return err;
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
    // ignore tests if FTP URL isn't set
    if (strlen(MBED_CONF_SIM5320_DRIVER_TEST_FTP_READ_WRITE_OPERATIONS_URL) == 0) {
        return STATUS_ABORT;
    }

    modem = new SIM5320(MBED_CONF_SIM5320_DRIVER_TEST_UART_TX, MBED_CONF_SIM5320_DRIVER_TEST_UART_RX, NC, NC, MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN);
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(modem->reset());
    modem->init();
    ThisThread::sleep_for(500);
    // set PIN if we have it
    const char *pin = MBED_CONF_SIM5320_DRIVER_TEST_SIM_PIN;
    if (strlen(pin) > 0) {
        modem->get_device()->set_pin(pin);
    }
    // run modem
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(modem->request_to_start());

    // connect to network
    CellularContext *cellular_context = modem->get_context();
    cellular_context->set_credentials(MBED_CONF_SIM5320_DRIVER_TEST_APN, MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME, MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD);
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(cellular_context->connect());

    ftp_client = modem->get_ftp_client();

    // connect to ftp server
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(ftp_client->connect(MBED_CONF_SIM5320_DRIVER_TEST_FTP_READ_WRITE_OPERATIONS_URL));

    // prepare test directory
    ABORT_TEST_SETUP_HANDLER_IF_ERROR(prepare_test_dir(ftp_client, test_dir));

    // create test file system
    block_device = new HeapBlockDevice(5120, 128);
    fs = new LittleFileSystem("heap", block_device);

    return greentea_test_setup_handler(number_of_cases);
}

void test_teardown_handler(const size_t passed, const size_t failed, const failure_t failure)
{
    // disconnect from ftp server
    if (ftp_client) {
        ftp_client->disconnect();
    }

    if (modem) {
        CellularContext *cellular_context = modem->get_context();
        cellular_context->disconnect();
        // stop modem (CellularDevise::shutdown)
        modem->request_to_stop();
    }

    // cleanup resources
    delete modem;
    // delete file system
    fs->unmount();
    delete fs;
    delete block_device;

    return greentea_test_teardown_handler(passed, failed, failure);
}

utest::v1::status_t case_setup_handler(const Case *const source, const size_t index_of_case)
{
    // clear file system
    fs->reformat(block_device);
    // cleanup test directory
    int err = ftp_client->rmtree(test_dir, false);

    status_t res = greentea_case_setup_handler(source, index_of_case);
    if (err) {
        res = STATUS_ABORT;
    }
    return res;
}

void test_listdir()
{
    int err;
    char path_buf[128];

    // create test file and directory
    sprintf(path_buf, "%s/%s", test_dir, "some_file.txt");
    err = ftp_client->put(path_buf, (uint8_t *)"abc", 3);
    TEST_ASSERT_EQUAL(0, err);
    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->mkdir(path_buf);
    TEST_ASSERT_EQUAL(0, err);

    // check listdir results
    SIM5320FTPClient::dir_entry_list_t dir_entry_list;
    SIM5320FTPClient::dir_entry_t *dir_entry_ptr;
    SIM5320FTPClient::dir_entry_t *dir_entry_dir_ptr;
    SIM5320FTPClient::dir_entry_t *dir_entry_file_ptr;
    err = ftp_client->listdir(test_dir, &dir_entry_list);
    TEST_ASSERT_EQUAL(0, err);

    // check that directory contains 2 entity
    int item_count = 0;
    for (dir_entry_ptr = dir_entry_list.get_head(); dir_entry_ptr != NULL; dir_entry_ptr = dir_entry_ptr->next) {
        item_count++;
    }
    TEST_ASSERT_EQUAL(2, item_count);
    if (item_count == 2) {
        dir_entry_ptr = dir_entry_list.get_head();
        if (dir_entry_ptr->d_type == DT_REG) {
            dir_entry_dir_ptr = dir_entry_ptr->next;
            dir_entry_file_ptr = dir_entry_ptr;
        } else {
            dir_entry_dir_ptr = dir_entry_ptr;
            dir_entry_file_ptr = dir_entry_ptr->next;
        }
        TEST_ASSERT_EQUAL(DT_REG, dir_entry_file_ptr->d_type);
        TEST_ASSERT_EQUAL_STRING("some_file.txt", dir_entry_file_ptr->name);
        TEST_ASSERT_EQUAL(DT_DIR, dir_entry_dir_ptr->d_type);
        TEST_ASSERT_EQUAL_STRING("some_dir", dir_entry_dir_ptr->name);
    }

    // check fail on no existed directory
    dir_entry_list.delete_all();
    sprintf(path_buf, "%s/%s", test_dir, "missed_dir");
    err = ftp_client->listdir(path_buf, &dir_entry_list);
    TEST_ASSERT_NOT_EQUAL(0, err);
}

void test_rmfile()
{
    int err;
    char path_buf[128];

    // create file and directory
    sprintf(path_buf, "%s/%s", test_dir, "some_file.txt");
    err = ftp_client->put(path_buf, (uint8_t *)"abc", 3);
    TEST_ASSERT_EQUAL(0, err);
    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->mkdir(path_buf);
    TEST_ASSERT_EQUAL(0, err);

    // test remove operation
    sprintf(path_buf, "%s/%s", test_dir, "some_file.txt");
    err = ftp_client->rmfile(path_buf);
    TEST_ASSERT_EQUAL(0, err);

    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->rmfile(path_buf);
    TEST_ASSERT_NOT_EQUAL(0, err);

    sprintf(path_buf, "%s/%s", test_dir, "some_file.txt");
    err = ftp_client->rmfile(path_buf);
    TEST_ASSERT_NOT_EQUAL(0, err);
}

void test_rmdir()
{
    int err;
    char path_buf[128];

    // create file and directory
    sprintf(path_buf, "%s/%s", test_dir, "some_file.txt");
    err = ftp_client->put(path_buf, (uint8_t *)"abc", 3);
    TEST_ASSERT_EQUAL(0, err);
    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->mkdir(path_buf);
    TEST_ASSERT_EQUAL(0, err);

    // test remove operation
    sprintf(path_buf, "%s/%s", test_dir, "some_file.txt");
    err = ftp_client->rmdir(path_buf);
    TEST_ASSERT_NOT_EQUAL(0, err);

    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->rmdir(path_buf);
    TEST_ASSERT_EQUAL(0, err);

    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->rmdir(path_buf);
    TEST_ASSERT_NOT_EQUAL(0, err);
}

void test_rmtree()
{
    int err;
    char path_buf[128];

    // create files and directories
    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->mkdir(path_buf);
    TEST_ASSERT_EQUAL(0, err);
    sprintf(path_buf, "%s/%s/%s", test_dir, "some_dir", "d1");
    err = ftp_client->mkdir(path_buf);
    TEST_ASSERT_EQUAL(0, err);
    sprintf(path_buf, "%s/%s/%s", test_dir, "some_dir", "f1.txt");
    err = ftp_client->put(path_buf, (uint8_t *)"1", 1);
    TEST_ASSERT_EQUAL(0, err);
    sprintf(path_buf, "%s/%s/%s/%s", test_dir, "some_dir", "d1", "f2.txt");
    err = ftp_client->put(path_buf, (uint8_t *)"2", 1);
    TEST_ASSERT_EQUAL(0, err);

    // test remove operation
    sprintf(path_buf, "%s/%s", test_dir, "some_dir");
    err = ftp_client->rmtree(path_buf);
    TEST_ASSERT_EQUAL(0, err);

    // check that directory doesn't exist
    bool res;
    err = ftp_client->exists(path_buf, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(res, false);
}

// Test function to get information about files on the server
void test_info_functions()
{
    int err;
    bool res;
    const size_t MAX_CWD_LEN = 32;
    char current_cwd[MAX_CWD_LEN];

    // create test files and directories
    char existed_file_path[48];
    char not_existed_file_path[48];
    char existed_dir_path[48];
    char not_existed_dir_path[48];
    sprintf(existed_file_path, "%s/%s", test_dir, "readme.txt");
    sprintf(not_existed_file_path, "%s/%s", test_dir, "fake.txt");
    sprintf(existed_dir_path, "%s/%s", test_dir, "some_dir");
    sprintf(not_existed_dir_path, "%s/%s", test_dir, "fake_dir");
    const char *init_cwd = "/";
    err = ftp_client->mkdir(existed_dir_path);
    TEST_ASSERT_EQUAL(0, err);
    err = ftp_client->put(existed_file_path, (uint8_t *)"123", 3);
    TEST_ASSERT_EQUAL(0, err);

    // check function to change current working directory
    err = ftp_client->set_cwd(existed_dir_path);
    TEST_ASSERT_EQUAL(0, err);
    err = ftp_client->get_cwd(current_cwd, MAX_CWD_LEN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(existed_dir_path, current_cwd);

    err = ftp_client->set_cwd(not_existed_dir_path);
    TEST_ASSERT_NOT_EQUAL(0, err);
    err = ftp_client->get_cwd(current_cwd, MAX_CWD_LEN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(existed_dir_path, current_cwd);

    err = ftp_client->set_cwd(init_cwd);
    TEST_ASSERT_EQUAL(0, err);

    // test isdir function
    err = ftp_client->isdir(existed_file_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    err = ftp_client->isdir(not_existed_file_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    err = ftp_client->isdir(existed_dir_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, res);
    err = ftp_client->isdir(not_existed_dir_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    // check that current directory isn't changed
    err = ftp_client->get_cwd(current_cwd, MAX_CWD_LEN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(init_cwd, current_cwd);

    // test isfile function
    err = ftp_client->isfile(existed_file_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, res);
    err = ftp_client->isfile(not_existed_file_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    err = ftp_client->isfile(existed_dir_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    err = ftp_client->isfile(not_existed_dir_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    // check that current directory isn't changed
    err = ftp_client->get_cwd(current_cwd, MAX_CWD_LEN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(init_cwd, current_cwd);

    // test exists function
    err = ftp_client->exists(existed_file_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, res);
    err = ftp_client->exists(not_existed_file_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    err = ftp_client->exists(existed_dir_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(true, res);
    err = ftp_client->exists(not_existed_dir_path, res);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(false, res);
    // check that current directory isn't changed
    err = ftp_client->get_cwd(current_cwd, MAX_CWD_LEN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(init_cwd, current_cwd);

    // test get_file_size function
    long file_size = 0;
    err = ftp_client->get_file_size(existed_file_path, file_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(file_size > 0);
    err = ftp_client->get_file_size(not_existed_file_path, file_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(file_size < 0);
    err = ftp_client->get_file_size(existed_dir_path, file_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(file_size < 0);
    err = ftp_client->get_file_size(not_existed_dir_path, file_size);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT(file_size < 0);
    // check that current directory isn't changed
    err = ftp_client->get_cwd(current_cwd, MAX_CWD_LEN);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING(init_cwd, current_cwd);
}

void test_upload_download_file()
{
    int err;
    FILE *file;
    char local_path[32];
    char remote_path[96];
    const size_t file_size = 2560;
    const char file_sym = '3';
    int sym;
    sprintf(remote_path, "%s/%s", test_dir, "demo_file.txt");
    sprintf(local_path, "/heap/%s", "demo_file.txt");

    // create local file
    file = fopen(local_path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    if (!file) {
        return;
    }
    for (size_t i = 0; i < file_size; i++) {
        err = fputc(file_sym, file);
        TEST_ASSERT_EQUAL(file_sym, err);
        if (err != file_sym) {
            return;
        }
    }
    err = fclose(file);
    TEST_ASSERT_EQUAL(0, err);
    if (err) {
        return;
    }

    // 1. Upload test file
    err = ftp_client->upload(local_path, remote_path);
    TEST_ASSERT_EQUAL(0, err);
    // 2. Remove local file
    err = remove(local_path);
    TEST_ASSERT_EQUAL(0, err);
    // 3. Download test file
    err = ftp_client->download(remote_path, local_path);
    TEST_ASSERT_EQUAL(0, err);
    // 4. Validate local file
    file = fopen(local_path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    if (!file) {
        return;
    }
    int total_len = 0;
    while ((sym = fgetc(file)) >= 0) {
        total_len++;
        TEST_ASSERT_EQUAL(file_sym, sym);
    }
    TEST_ASSERT_EQUAL(file_size, total_len);
    err = fclose(file);
    TEST_ASSERT_EQUAL(0, err);
    if (err) {
        return;
    }
}

// test cases description
#define SIM5320Case(test_fun) Case(#test_fun, case_setup_handler, test_fun, greentea_case_teardown_handler, greentea_case_failure_continue_handler)
Case cases[] = {
    SIM5320Case(test_listdir),
    SIM5320Case(test_rmfile),
    SIM5320Case(test_rmdir),
    SIM5320Case(test_rmtree),
    SIM5320Case(test_info_functions),
    SIM5320Case(test_upload_download_file)

};
Specification specification(test_setup_handler, cases, test_teardown_handler);

// Entry point into the tests
int main()
{
    // base config validation
    validate_test_pins(true, true, false);
    validate_test_ftp_settings(false, true);

    // host handshake
    // note: it should be invoked here or in the test_setup_handler
    GREENTEA_SETUP(300, "default_auto");
    // run tests
    return !Harness::run(specification);
}
