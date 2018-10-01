#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularStack.h"
#include "sim5320_utils.h"
using namespace sim5320;

SIM5320CellularNetwork::SIM5320CellularNetwork(ATHandler& atHandler)
    : AT_CellularNetwork(atHandler)
{
}

SIM5320CellularNetwork::~SIM5320CellularNetwork()
{
}

nsapi_error_t SIM5320CellularNetwork::init()
{
    nsapi_error_t err = AT_CellularNetwork::init();
    if (err) {
        return err;
    }
    _at.lock();

    // set automatic selection between GSM and WCDMA
    // NOTE: the command can fails, so ignore any error
    int err_code = _at.get_last_error();
    _at.cmd_start("AT+CNMP=2");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.clear_error();

    _at.unlock();
    return err_code;
}

#define DEFAULT_PDP_CONTEXT_NO 1

nsapi_error_t SIM5320CellularNetwork::activate_context()
{
    // check that apn is set
    if (!_apn) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    // configure context number 1 to use it
    const char* stack_type;
    switch (_ip_stack_type_requested) {
    case IPV4_STACK:
        stack_type = "IP";
        break;
    case IPV6_STACK:
        stack_type = "IPV6";
        break;
    case DEFAULT_STACK:
        // use IPV4 by default
        _ip_stack_type_requested = IPV4_STACK;
        stack_type = "IP";
        break;
    default:
        return NSAPI_ERROR_NO_CONNECTION;
    }
    _ip_stack_type = _ip_stack_type_requested;

    _at.lock();

    // configure context
    _at.cmd_start("AT+CGDCONT=");
    _at.write_int(DEFAULT_PDP_CONTEXT_NO);
    _at.write_string(stack_type);
    _at.write_string(_apn);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.cmd_start("AT+CGSOCKCONT=");
    _at.write_int(DEFAULT_PDP_CONTEXT_NO);
    _at.write_string(stack_type);
    _at.write_string(_apn);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    nsapi_error_t err = _at.unlock_return_error();
    if (err) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    // run other activation code
    err = AT_CellularNetwork::activate_context();
    if (err) {
        _at.unlock();
        return NSAPI_ERROR_NO_CONNECTION;
    }

    // set active context as default for sockets
    _at.cmd_start("AT+CSOCKSETPN=");
    _at.write_int(_cid);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    return _at.unlock_return_error();
}

#define NET_OPEN_CLOSE_TIMEOUT 30000
#define NET_OPEN_CLOSE_CHECK_DELAY 5000

nsapi_error_t SIM5320CellularNetwork::wait_network_status(int expected_status, int timeout)
{
    Timer timer;
    int status = -1;
    nsapi_error_t err = NSAPI_ERROR_OK;

    timer.start();
    _at.lock();
    if (_at.get_last_error()) {
        return _at.unlock_return_error();
    }

    while (1) {
        _at.clear_error();
        _at.cmd_start("AT+NETOPEN?");
        _at.cmd_stop();
        _at.resp_start("+NETOPEN: ");
        status = _at.read_int();
        _at.resp_stop();
        if (status == expected_status) {
            break;
        }
        if (timer.read_ms() > NET_OPEN_CLOSE_TIMEOUT) {
            break;
            err = NSAPI_ERROR_TIMEOUT;
        }
        wait_ms(NET_OPEN_CLOSE_CHECK_DELAY);
    }
    _at.unlock();
    return err;
}

nsapi_error_t SIM5320CellularNetwork::connect()
{
    _at.lock();
    nsapi_error_t err = AT_CellularNetwork::connect();
    if (err) {
        return _at.unlock_return_error();
    }
    // set command mode (non-transparent mode)
    _at.cmd_start("AT+CIPMODE=0");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    // activate stack
    _at.cmd_start("AT+NETOPEN");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    err = wait_network_status(1, NET_OPEN_CLOSE_TIMEOUT);
    return any_error(err, _at.unlock_return_error());
}

nsapi_error_t SIM5320CellularNetwork::disconnect()
{
    nsapi_error_t err_1, err_2;
    _at.lock();
    // deactivate stack
    _at.cmd_start("AT+NETCLOSE");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    err_1 = wait_network_status(0, NET_OPEN_CLOSE_TIMEOUT);
    err_2 = AT_CellularNetwork::disconnect();
    _at.unlock();
    return any_error(err_1, err_2);
}

static const CellularNetwork::RadioAccessTechnology RAT_TYPE_CNSMOD_CODE_MAP[] = {
    CellularNetwork::RAT_UNKNOWN,
    CellularNetwork::RAT_GSM,
    CellularNetwork::RAT_GSM, // FIXME: GPRS by docs
    CellularNetwork::RAT_EGPRS,
    CellularNetwork::RAT_UTRAN, // FIXME: WCDMA by docs
    CellularNetwork::RAT_HSDPA,
    CellularNetwork::RAT_HSUPA,
    CellularNetwork::RAT_HSDPA_HSUPA,
};

nsapi_error_t SIM5320CellularNetwork::get_access_technology(CellularNetwork::RadioAccessTechnology& rat)
{
    _at.lock();
    // get current network_type
    _at.cmd_start("AT+CNSMOD?");
    _at.cmd_stop();
    _at.resp_start("+CNSMOD:");
    _at.skip_param(); // ignore network report mode
    int rat_type = _at.read_int();
    _at.resp_stop();
    if (rat_type < 0 || rat_type > 7) {
        rat_type = 0;
    }
    rat = RAT_TYPE_CNSMOD_CODE_MAP[rat_type];
    return _at.unlock_return_error();
}

nsapi_error_t SIM5320CellularNetwork::get_registration_status(CellularNetwork::RegistrationType type, CellularNetwork::RegistrationStatus& status)
{
    switch (type) {
    case CellularNetwork::C_GREG:
    case CellularNetwork::C_REG:
        return AT_CellularNetwork::get_registration_status(type, status);
    default:
        return NSAPI_ERROR_UNSUPPORTED;
    }
}

nsapi_error_t SIM5320CellularNetwork::get_extended_signal_quality(int& rxlev, int& ber, int& rscp, int& ecno, int& rsrq, int& rsrp)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SIM5320CellularNetwork::set_ciot_optimization_config(CellularNetwork::Supported_UE_Opt supported_opt, CellularNetwork::Preferred_UE_Opt preferred_opt)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SIM5320CellularNetwork::get_ciot_optimization_config(CellularNetwork::Supported_UE_Opt& supported_opt, CellularNetwork::Preferred_UE_Opt& preferred_opt)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

NetworkStack* SIM5320CellularNetwork::get_stack()
{

    if (!_stack) {
        _stack = new SIM5320CellularStack(_at, _cid, _ip_stack_type);
    }
    return _stack;
}

bool SIM5320CellularNetwork::get_modem_stack_type(nsapi_ip_stack_t requested_stack)
{
    switch (requested_stack) {
    case IPV4_STACK:
    case IPV6_STACK:
        return true;
    default:
        return false;
    }
}

nsapi_error_t SIM5320CellularNetwork::do_user_authentication()
{
    // set username and password
    if (_pwd && _uname) {
        // configure CGAUTH
        _at.cmd_start("AT+CGAUTH=");
        _at.write_int(_cid);
        _at.write_int(1);
        _at.write_string(_pwd);
        _at.write_string(_uname);
        _at.cmd_stop();
        _at.resp_start();
        _at.resp_stop();
        // configure SOCKET authentication
        _at.cmd_start("AT+CSOCKAUTH=");
        _at.write_int(_cid);
        _at.write_int(1);
        _at.write_string(_pwd);
        _at.write_string(_uname);
        _at.cmd_stop();
        _at.resp_start();
        _at.resp_stop();
        if (_at.get_last_error() != NSAPI_ERROR_OK) {
            return NSAPI_ERROR_AUTH_FAILURE;
        }
    }
    return NSAPI_ERROR_OK;
}
