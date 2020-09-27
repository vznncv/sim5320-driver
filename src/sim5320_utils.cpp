#include "mbed_assert.h"

#include "sim5320_utils.h"

#include <limits.h>
#include <string.h>

using namespace sim5320;

int sim5320::read_full_fuzzy_response(ATHandler &at, bool wait_response_after_ok, bool wait_response_after_error, const char *prefix, const char *format_string, ...)
{
    ssize_t result;
    va_list args;
    va_start(args, format_string);
    result = vread_full_fuzzy_response(at, wait_response_after_ok, wait_response_after_error, prefix, format_string, args);
    va_end(args);
    return result;
}

#define DEFAULT_MAX_STRING_LENGTH 64

static int vread_full_fuzzy_response_values(mbed::ATHandler &at, const char *format_string, va_list arg)
{
    int result = 0;
    char current_sym;
    bool format_seq = false;
    int read_res;

    while ((current_sym = *(format_string++)) != '\0') {
        if (format_seq) {
            if (current_sym == 'i' || current_sym == 'd') {
                int *dst_int_ptr = va_arg(arg, int *);

                read_res = at.read_int();
                if (read_res < 0) {
                    return result;
                }
                *dst_int_ptr = read_res;
                result++;

            } else if (current_sym == 's') {
                char *dst_str_ptr = va_arg(arg, char *);
                // TODO: add support of the max string length
                read_res = at.read_string(dst_str_ptr, DEFAULT_MAX_STRING_LENGTH);
                if (read_res < 0) {
                    return result;
                }
                result++;

            } else {
                return NSAPI_ERROR_PARAMETER;
            }
            format_seq = false;
        } else {
            if (current_sym != '%') {
                return NSAPI_ERROR_PARAMETER;
            }
            format_seq = true;
        }
    }
    return result;
}

int sim5320::vread_full_fuzzy_response(ATHandler &at, bool wait_response_after_ok, bool wait_response_after_error, const char *prefix, const char *format_string, va_list arg)
{
    int err;
    int result = 0;
    at.resp_start(prefix);
    if (at.info_resp()) {
        // the command is matched at first
        result = vread_full_fuzzy_response_values(at, format_string, arg);
        // try to reach "OK" or "ERROR"
        at.resp_start();
        at.resp_stop();
        err = at.get_last_error();
    } else if (at.get_last_error()) {
        // the "ERROR" is matched at first
        err = at.get_last_error();
        if (wait_response_after_error) {
            at.clear_error();
            // try to read command again, but ignore results
            at.resp_start(prefix);
            at.consume_to_stop_tag();
        }
    } else {
        if (wait_response_after_ok) {
            // the "OK" is matched at first
            // try to read command again
            at.resp_start(prefix);
            result = vread_full_fuzzy_response_values(at, format_string, arg);
            err = at.get_last_error();
            at.consume_to_stop_tag();
        } else {
            err = 0;
        }
    }

    return err ? err : result;
}

ATHandlerLocker::ATHandlerLocker(ATHandler &at, mbed::chrono::milliseconds_u32 timeout)
    : _at(at)
    , _timeout(timeout)
    , _lock_count(1)
{
    _at.lock();
    if (timeout > 0ms) {
        _at.set_at_timeout(timeout);
    }
}

sim5320::ATHandlerLocker::~ATHandlerLocker()
{
    if (_timeout > 0ms) {
        _at.restore_at_timeout();
    }
    for (int i = 0; i < _lock_count; i++) {
        _at.unlock();
    }
}

void sim5320::ATHandlerLocker::reset_timeout()
{
    _at.lock();
    _lock_count++;
}

/**
 * Build AT command.
 *
 * @param buf fixed char array for output command
 * @param prefix command prefix (string literal)
 * @param cmd command (ordinary c string)
 * @param suffix command suffix (string literal)
 * @return 0 on success, otherwise non-zero value
 */
template <int buf_len, int prefix_len, int suffix_len>
static inline int at_cmdw_build(char (&buf)[buf_len], const char (&prefix)[prefix_len], const char *cmd, const char (&suffix)[suffix_len])
{
    // notes:
    // - prefix_len includes string length and string terminator symbol '\0'
    // - suffix_len includes string length and string terminator symbol '\0'
    size_t cmd_len = strlen(cmd);
    size_t target_len = prefix_len + cmd_len + suffix_len - 2;
    if (target_len + 1 > buf_len) {
        return MBED_ERROR_CODE_EMSGSIZE;
    }

    strcpy(buf, prefix);
    strcpy(buf + prefix_len - 1, cmd);
    strcpy(buf + prefix_len - 1 + cmd_len, suffix);
    buf[target_len] = '\0';
    return 0;
}

static inline void at_cmdw_lock(ATHandler &at, bool lock)
{
    if (lock) {
        at.lock();
    }
}

static inline int at_cmdw_unlock_return_error(ATHandler &at, bool lock)
{
    if (lock) {
        return at.unlock_return_error();
    } else {
        return at.get_last_error();
    }
}

nsapi_error_t sim5320::at_cmdw_run(ATHandler &at, const char *cmd, bool lock)
{
    char cmd_buf[20];
    int err;

    // build command
    if ((err = at_cmdw_build(cmd_buf, "AT", cmd, ""))) {
        return err;
    }

    at_cmdw_lock(at, lock);

    at.cmd_start(cmd_buf);
    at.cmd_stop_read_resp();

    return at_cmdw_unlock_return_error(at, lock);
}

nsapi_error_t sim5320::at_cmdw_set_i(ATHandler &at, const char *cmd, int value, bool lock)
{
    char cmd_buf[20];
    int err;

    // build command
    if ((err = at_cmdw_build(cmd_buf, "AT", cmd, "="))) {
        return err;
    }

    at_cmdw_lock(at, lock);

    at.cmd_start(cmd_buf);
    at.write_int(value);
    at.cmd_stop_read_resp();

    return at_cmdw_unlock_return_error(at, lock);
}

nsapi_error_t sim5320::at_cmdw_get_i(ATHandler &at, const char *cmd, int &value, bool lock)
{
    char cmd_buf[20];
    int err;

    // build command
    if ((err = at_cmdw_build(cmd_buf, "AT", cmd, "?"))) {
        return err;
    }

    at_cmdw_lock(at, lock);

    at.cmd_start(cmd_buf);
    at.cmd_stop();
    // replace "?" with ":"
    cmd_buf[strlen(cmd_buf) - 1] = ':';
    at.resp_start(cmd_buf + 2); // skip "AT" part in the cmd_buf
    value = at.read_int();
    at.resp_stop();

    return at_cmdw_unlock_return_error(at, lock);
}

nsapi_error_t sim5320::at_cmdw_set_ii(ATHandler &at, const char *cmd, int value_1, int value_2, bool lock)
{
    char cmd_buf[20];
    int err;

    // build command
    if ((err = at_cmdw_build(cmd_buf, "AT", cmd, "="))) {
        return err;
    }

    at_cmdw_lock(at, lock);

    at.cmd_start(cmd_buf);
    at.write_int(value_1);
    at.write_int(value_2);
    at.cmd_stop_read_resp();

    return at_cmdw_unlock_return_error(at, lock);
}

nsapi_error_t sim5320::at_cmdw_get_ii(ATHandler &at, const char *cmd, int &value_1, int &value_2, bool lock)
{
    char cmd_buf[20];
    int err;

    // build command
    if ((err = at_cmdw_build(cmd_buf, "AT", cmd, "?"))) {
        return err;
    }

    at_cmdw_lock(at, lock);

    at.cmd_start(cmd_buf);
    at.cmd_stop();
    // replace "?" with ":"
    cmd_buf[strlen(cmd_buf) - 1] = ':';
    at.resp_start(cmd_buf + 2); // skip "AT" part in the cmd_buf
    value_1 = at.read_int();
    value_2 = at.read_int();
    at.resp_stop();

    return at_cmdw_unlock_return_error(at, lock);
}

SimpleStringParser::SimpleStringParser(const char *str)
    : _str(str)
    , _err(0)
{
    MBED_ASSERT(str != nullptr);
}

int SimpleStringParser::consume_int(int *result, int limit)
{
    const char *pos = _str;
    int res = 0;
    bool neg_sign = false;
    bool has_digits = false;

    if (_err) {
        return _err;
    }

    if (limit < 0) {
        limit = INT_MAX;
    }

    // check sign
    if (*pos == '+' || *pos == '-') {
        pos++;
        limit--;
        if (*pos == '-') {
            neg_sign = true;
        }
    }
    // process number
    while (*pos >= '0' && *pos <= '9' && limit > 0) {
        res = res * 10 + (*pos - '0');
        pos++;
        limit--;
        has_digits = true;
    }
    // check results
    if (!has_digits) {
        _err = -1;
    } else {
        if (neg_sign) {
            res = -res;
        }
        *result = res;
        _str = pos;
    }
    return _err;
}

int SimpleStringParser::consume_literal(const char *str)
{
    const char *pos = _str;

    if (_err) {
        return _err;
    }

    while (*str != '\0') {
        if (*str != *pos) {
            _err = -1;
            break;
        }
        str++;
        pos++;
    }
    if (!_err) {
        _str = pos;
    }
    return _err;
}

int SimpleStringParser::consume_char(char *sym)
{
    if (_err) {
        return _err;
    }

    if (*_str == '\0') {
        _err = -1;
        return _err;
    }

    *sym = *_str;
    _str++;

    return _err;
}

int SimpleStringParser::consume_string_until_sep(char *buf, size_t len, char sep)
{
    const char *pos = _str;
    int res;

    MBED_ASSERT(len > 0);

    if (_err) {
        return _err;
    }

    while (*pos != '\0' && len > 1 && *pos != sep) {
        *buf = *pos;
        buf++;
        pos++;
        len--;
    }
    *buf = '\0';

    res = pos - _str;
    _str = pos;
    return res;
}

int SimpleStringParser::get_error()
{
    return _err;
}

bool SimpleStringParser::is_finshed()
{
    return *_str == '\0';
}
