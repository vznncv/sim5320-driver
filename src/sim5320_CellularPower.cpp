#include "sim5320_CellularPower.h"
using namespace sim5320;

SIM5320CellularPower::SIM5320CellularPower(ATHandler& atHandler)
    : AT_CellularPower(atHandler)
{
}

SIM5320CellularPower::~SIM5320CellularPower()
{
}

nsapi_error_t SIM5320CellularPower::exec_cfun(int func_level)
{

}

#define RESET_TIMEOUT_MS 30000
#define STARTUP_TIMEOUT_MS 30000

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
    }

    // wait device startup, but ignore any errors
    _skip_initialization_messages(STARTUP_TIMEOUT_MS);
    return NSAPI_ERROR_OK;
}

#define CFUN_TIMEOUT 8000

nsapi_error_t SIM5320CellularPower::set_power_level(int func_level, int do_reset)
{
    int err_code;
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
    err_code = _at.get_last_error();

    if (func_level == 1) {
        // wait end of startup, it's it can show some output that can break other output
        // FIXME: check if this action is required
        _skip_initialization_messages(60000);
    }

    _at.unlock();
    return err_code;
}



void SIM5320CellularPower::_skip_initialization_messages(int timeout)
{
    _at.set_at_timeout(timeout);
    _at.resp_start("START", true);
    // if there is not error, wait PB DONE
    _at.resp_start("PB DONE", true);
    _at.restore_at_timeout();
    // clear any error codes of this step
    _at.clear_error();
}

nsapi_error_t sim5320::SIM5320CellularPower::opt_power_save_mode()
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t sim5320::SIM5320CellularPower::opt_receive_period(int mode, CellularPower::EDRXAccessTechnology act_type, uint8_t edrx_value)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
