#ifndef SIM5320_TESTS_UTILS_H
#define SIM5320_TESTS_UTILS_H
/**
 * This is a private header that is designed only for internal tests of the sim5320.
 */

#include "mbed.h"
#include "string.h"
#include "utest.h"

namespace sim5320 {

inline void validate_test_pins(bool require_rx = true, bool require_tx = true, bool require_reset = true)
{
    if (require_rx && MBED_CONF_SIM5320_DRIVER_TEST_UART_TX == NC) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_uart_rx must be set to run tests");
    }
    if (require_tx && MBED_CONF_SIM5320_DRIVER_TEST_UART_TX == NC) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_uart_tx must be set to run tests");
    }
    if (require_reset && MBED_CONF_SIM5320_DRIVER_TEST_RESET_PIN == NC) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_reset_pin must be set to run tests");
    }
}

inline void validate_test_apn_settings()
{
    const volatile size_t apn_len = strlen(MBED_CONF_SIM5320_DRIVER_TEST_APN);
    if (apn_len == 0) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_apn must be set to run tests");
    }
    if (MBED_CONF_SIM5320_DRIVER_TEST_APN_USERNAME == nullptr) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_apn_username must be set to run tests");
    }
    if (MBED_CONF_SIM5320_DRIVER_TEST_APN_PASSWORD == nullptr) {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_apn_password must be set to run tests");
    }
}

inline void validate_test_ftp_settings(bool require_read_conf = true, bool require_write_conf = true)
{
    const size_t ftp_read_url_len = strlen(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTP_URL);
    const size_t ftp_read_expl_url_len = strlen(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTPS_EXPLICIT_URL);
    const size_t ftp_read_impl_url_len = strlen(MBED_CONF_SIM5320_DRIVER_TEST_FTP_CONNECT_FTPS_IMPLICIT_URL);
    const size_t ftp_write_url_len = strlen(MBED_CONF_SIM5320_DRIVER_TEST_FTP_READ_WRITE_OPERATIONS_URL);
    const size_t ftp_write_dir_len = strlen(MBED_CONF_SIM5320_DRIVER_TEST_FTP_READ_WRITE_OPERATIONS_DIR);
    if (require_read_conf) {
        if (ftp_read_url_len == 0) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_ftp_connect_ftp_url must be set to run tests");
        }
        if (ftp_read_expl_url_len == 0) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_ftp_connect_ftps_explicit_url must be set to run tests");
        }
        if (ftp_read_impl_url_len == 0) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_ftp_connect_ftps_implicit_url must be set to run tests");
        }
    }
    if (require_write_conf) {
        if (ftp_write_url_len == 0) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_ftp_read_write_operations_url must be set to run tests");
        }
        if (ftp_write_dir_len == 0) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_ASSERTION_FAILED), "sim5320-driver.test_ftp_read_write_operations_dir must be set to run tests");
        }
    }
}

inline utest::v1::status_t unite_utest_status(utest::v1::status_t s1, utest::v1::status_t s2)
{
    if (s1 == utest::v1::STATUS_ABORT || s2 == utest::v1::STATUS_ABORT) {
        return utest::v1::STATUS_ABORT;
    }
    if (s2 == utest::v1::STATUS_IGNORE || s2 == utest::v1::STATUS_IGNORE) {
        return utest::v1::STATUS_IGNORE;
    }
    return s1;
}

inline utest::v1::status_t unite_utest_status_with_err(utest::v1::status_t status, int err, utest::v1::status_t err_status = utest::v1::STATUS_ABORT)
{
    if (err) {
        return err_status;
    }
    return status;
}

inline int any_error(int err_1, int err_2)
{
    if (err_1) {
        return err_1;
    }
    return err_2;
}

inline bool not_empty(const char *str)
{
    return strlen(str) > 0;
}
}

#endif // SIM5320_TESTS_UTILS_H
