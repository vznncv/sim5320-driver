#include "sim5320_CellularPower.h"
#include "sim5320_utils.h"
using namespace sim5320;

SIM5320CellularPower::SIM5320CellularPower(ATHandler& at_handler)
    : AT_CellularPower(at_handler)
{
}

SIM5320CellularPower::~SIM5320CellularPower()
{
}

#define CFUN_TIMEOUT 8000

#define STARTUP_TIMEOUT_MS 32000

nsapi_error_t SIM5320CellularPower::reset()
{
    _at.lock();
    // send reset command
    _at.cmd_start("AT+CRESET");
    _at.cmd_stop();
    // check OK response
    _at.resp_start();
    _at.resp_stop();
    // if error occurs then stop action
    if (_at.get_last_error()) {
        return _at.unlock_return_error();
    } else {
        _at.unlock();
    }
    // wait device startup messages
    _skip_initialization_messages(STARTUP_TIMEOUT_MS);
    // force at mode
    set_at_mode();

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularPower::set_power_level(int func_level, int do_reset)
{
    // limit support by energy levels 0 an 1
    if (func_level < 0 || func_level > 1) {
        return NSAPI_ERROR_PARAMETER;
    }
    _at.lock();
    _at.set_at_timeout(CFUN_TIMEOUT);
    _at.cmd_start("AT+CFUN=");
    _at.write_int(func_level);
    _at.write_int(do_reset);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.restore_at_timeout();
    return _at.unlock_return_error();
}

void SIM5320CellularPower::_skip_initialization_messages(int timeout)
{
    _at.lock();
    _at.set_at_timeout(timeout);
    _at.resp_start("START", true);
    _at.clear_error();
    // if there is not error, wait PB DONE
    _at.resp_start("PB DONE", true);
    _at.clear_error();
    _at.restore_at_timeout();
    // clear any error codes of this step
    _at.unlock();
}

nsapi_error_t sim5320::SIM5320CellularPower::opt_power_save_mode(int periodic_time, int active_time)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t sim5320::SIM5320CellularPower::opt_receive_period(int mode, CellularPower::EDRXAccessTechnology act_type, uint8_t edrx_value)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
