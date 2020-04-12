#ifndef SIM5320_UTILS_H
#define SIM5320_UTILS_H
#include "ATHandler.h"
#include "CellularLog.h"
#include "mbed.h"
#include "stdarg.h"
namespace sim5320 {

static const int SIM5320_DEFAULT_TIMEOUT = 8000;

/**
 * Helper function that returns first found error code.
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
 * @param wait_response_after_ok if it's @c true, then wait response after "OK"
 * @param wait_response_after_error if it's @c true, then wait response after "ERROR", but ignore codes
 * @param prefix command prefix ("+CMD:")
 * @param format_string description of the arguments to read
 * @return number of successfully read arguments, or negative code in case of error.
 */
int read_full_fuzzy_response(ATHandler &at, bool wait_response_after_ok, bool wait_response_after_error, const char *prefix, const char *format_string, ...);

/**
 * Version of the @c read_full_fuzzy_response with a @c va_list argument.
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
 * Helper wrapper to execute simple AT command that accept and returns nothing.
 *
 * @param at at @c ATHandler object
 * @param cmd cmd command without "AT" prefix
 * @param lock lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
nsapi_error_t at_cmdw_run(ATHandler &at, const char *cmd, bool lock = true);

/**
 * Helper wrapper to execute simple AT command that returns nothing, but accepts one integer value.
 *
 * @param at @c ATHandler object
 * @param cmd command without "AT" prefix
 * @param value target number
 * @param lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
nsapi_error_t at_cmdw_set_i(ATHandler &at, const char *cmd, int value, bool lock = true);

/**
 * Helper wrapper to execute simple AT command that accepts nothing, but returns one integer value.
 *
 * @param at @c ATHandler object
 * @param cmd command without "AT" prefix
 * @param value returned value
 * @param lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
nsapi_error_t at_cmdw_get_i(ATHandler &at, const char *cmd, int &value, bool lock = true);

/**
 * Helper wrapper to execute simple AT command that returns nothing, but accepts boolean value (0 or 1).
 *
 * @param at @c ATHandler object
 * @param cmd command without "AT" prefix
 * @param value value
 * @param lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
inline nsapi_error_t at_cmdw_set_b(ATHandler &at, const char *cmd, bool value, bool lock = true)
{
    return at_cmdw_set_i(at, cmd, value ? 1 : 0, lock);
}

/**
 * Helper wrapper to execute simple AT command that accepts nothing, but returns one boolean value (0 or 1).
 *
 * @param at @c ATHandler object
 * @param cmd command without "AT" prefix
 * @param value returned value
 * @param lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
inline nsapi_error_t at_cmdw_get_b(ATHandler &at, const char *cmd, bool &value, bool lock = true)
{
    int err, value_i;
    err = at_cmdw_get_i(at, cmd, value_i, lock);
    if (err) {
        return err;
    }
    if (value_i == 0) {
        value = false;
        err = 0;
    } else if (value_i == 1) {
        value = true;
        err = 0;
    } else {
        // unknown value
        err = -1;
    }
    return err;
}

/**
 * Helper wrapper to execute simple AT command that returns nothing, but accepts two integer value.
 *
 * @param at @c ATHandler object
 * @param cmd command without "AT" prefix
 * @param value_1 first number
 * @param value_2 second number
 * @param lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
nsapi_error_t at_cmdw_set_ii(ATHandler &at, const char *cmd, int value_1, int value_2, bool lock = true);

/**
 * Helper wrapper to execute simple AT command that accepts nothing, but returns tow integer values.
 *
 * @param at @c ATHandler object
 * @param cmd command without "AT" prefix
 * @param value_1 first value
 * @param value_2 second value
 * @param lock ATHandler lock flag
 * @return 0 on success, otherwise non-zero value
 */
nsapi_error_t at_cmdw_get_ii(ATHandler &at, const char *cmd, int &value_1, int &value_2, bool lock = true);

/**
 * Helper object to lock @c ATHandler object using RAII approach.
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

/**
 * Helper class to manage objects with RAII approach.
 *
 * It's created for internal usage and shouldn't be used directly.
 */
//template <typename TObj, typename TAMRet, TAMRet (TObj::*acquire_method)(), typename TRMRet, TRMRet (TObj::*release_method)()>
//class BaseGuard {
//private:
//    BaseGuard(BaseGuard const &) = delete;
//    BaseGuard &operator=(BaseGuard const &) = delete;
//    TObj *_obj;
//public:
//    BaseGuard(TObj *obj)
//        : _obj(obj)
//    {
//        (*_obj.*acquire_method)();
//    }
//    BaseGuard(TObj &obj)
//        : BaseGuard(&obj)
//    {
//    }
//    ~BaseGuard()
//    {
//        (*_obj.*release_method)();
//    }
//};
}
#endif // SIM5320_UTILS_H
