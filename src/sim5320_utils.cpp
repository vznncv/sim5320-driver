#include "sim5320_utils.h"

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
            if (current_sym == 'i') {
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

sim5320::ATHandlerLocker::ATHandlerLocker(ATHandler &at, int timeout)
    : _at(at)
    , _timeout(timeout)
    , _lock_count(1)
{
    _at.lock();
    if (timeout >= 0) {
        _at.set_at_timeout(timeout);
    }
}

sim5320::ATHandlerLocker::~ATHandlerLocker()
{
    if (_timeout >= 0) {
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
