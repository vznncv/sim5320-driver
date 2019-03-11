#ifndef SIM5320_UTILS_H
#define SIM5320_UTILS_H
#include "ATHandler.h"
#include "CellularLog.h"
#include "mbed.h"
#include "stdarg.h"
namespace sim5320 {

/**
 * Helper function that to return first found error code.
 *
 * @param err_1
 * @param err_2
 * @return
 */
inline nsapi_error_t any_error(nsapi_error_t err_1, nsapi_error_t err_2)
{
    if (err_1) {
        return err_1;
    }
    if (err_2) {
        return err_2;
    }
    return 0;
}

#define SIM5320_UNLOCK_RETURN_IF_ERROR(at_handler_ptr) \
    if (at_handler_ptr->get_last_error()) {            \
        return at_handler_ptr->unlock_return_error();  \
    }

/**
 * Read AT response that:
 * - can have an information response (+CMD: val), even if error occurs
 * - has undefined order of OK/ERROR and information response (+CMD: val)
 *
 * Response samples:
 *
 * +CMD: 0, 2
 * OK
 *
 * OK
 * +CMD: 0, 2
 *
 * +CMD: 8, 2
 * ERROR
 *
 * ERROR
 * +CMD: 8, 2
 *
 * ERROR
 *
 * The function accept @p format_string string that describes command parameters to read. The @p format_string has @c scanf like syntax,
 * but it can contain only "%i" and "%s" to read positive integer and string.
 *
 * @param at @c ATHandler object
 * @param wait_response_after_error if it's @c true, then wait response after "ERROR", but ignore codes
 * @param wait_response_after_error if it's @c true, then wait response after "OK"
 * @param prefix command prefix
 * @param format_string description of the arguments to read
 * @return number of successfully read arguments, or negative code in case of error.
 */
int read_full_fuzzy_response(ATHandler &at, bool wait_response_after_ok, bool wait_response_after_error, const char *prefix, const char *format_string, ...);

/**
 * Version of the @c read_full_fuzzy_response with a variable list argument.
 *
 * @param at
 * @param wait_response_after_error
 * @param prefix
 * @param format_string
 * @param arg
 * @return
 */
int vread_full_fuzzy_response(ATHandler &at, bool wait_response_after_ok, bool wait_response_after_error, const char *prefix, const char *format_string, va_list arg);

/**
 * Helper object to lock @c ATHandler object using RAII approach.
 *
 * @brief The ATHandlerLock class
 */
class ATHandlerLocker {
public:
    ATHandlerLocker(ATHandler &at, int timeout = -1);
    ~ATHandlerLocker();

    /**
     * Reset at handler timeout.
     *
     * @note the reset operation clear handler errors.
     */
    void reset_timeout();

private:
    ATHandler &_at;
    int _timeout;
    int _lock_count;
};
}
#endif // SIM5320_UTILS_H
