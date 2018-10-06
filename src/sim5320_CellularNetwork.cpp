#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularStack.h"
#include "sim5320_utils.h"
using namespace sim5320;

SIM5320CellularNetwork::SIM5320CellularNetwork(ATHandler& at_handler)
    : AT_CellularNetwork(at_handler)
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
#define CONTEXT_ACTIVATION_TIMEOUT 16000

nsapi_error_t SIM5320CellularNetwork::activate_context()
{
    nsapi_error_t err;

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

    // configure context 1
    _cid = DEFAULT_PDP_CONTEXT_NO;
    _new_context_set = false;

    _at.cmd_start("AT+CGDCONT=");
    _at.write_int(_cid);
    _at.write_string(stack_type);
    _at.write_string(_apn);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.cmd_start("AT+CGSOCKCONT=");
    _at.write_int(_cid);
    _at.write_string(stack_type);
    _at.write_string(_apn);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    err = _at.get_last_error();
    if (err) {
        return _at.unlock_return_error();
    }
    // set user credentials
    err = do_user_authentication();
    if (err != NSAPI_ERROR_OK) {
        return _at.unlock_return_error();
    }
    // activate context
    _at.set_at_timeout(CONTEXT_ACTIVATION_TIMEOUT);
    _at.cmd_start("AT+CGACT=1,");
    _at.write_int(_cid);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.restore_at_timeout();
    if (!_at.get_last_error()) {
        _is_context_active = true;
    }

    // set active context as default for sockets
    _at.cmd_start("AT+CSOCKSETPN=");
    _at.write_int(_cid);
    _at.cmd_stop();
    _at.set_at_timeout(CONTEXT_ACTIVATION_TIMEOUT);
    _at.resp_start();
    _at.resp_stop();
    _at.restore_at_timeout();
    return _at.unlock_return_error();
}

#define NET_OPEN_CLOSE_TIMEOUT 30000
#define NET_OPEN_CLOSE_CHECK_DELAY 5000

nsapi_error_t SIM5320CellularNetwork::_wait_network_status(int expected_status, int timeout)
{
    Timer timer;
    int status = -1;
    nsapi_error_t err = _at.get_last_error();
    if (err) {
        return err;
    }

    timer.start();

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
    return err;
}

nsapi_error_t SIM5320CellularNetwork::connect()
{
    nsapi_error_t err_1, err_2;
    err_1 = AT_CellularNetwork::connect();
    if (err_1) {
        return err_1;
    }
    _at.lock();
    // don't show prompt with remove IP when new data is received
    _at.cmd_start("AT+CIPSRIP=0");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    // set command mode (non-transparent mode)
    _at.cmd_start("AT+CIPMODE=0");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    // set manual data receive mode
    _at.cmd_start("AT+CIPRXGET=1");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    // configure receive urc: "+RECEIVE"
    _at.cmd_start("AT+CIPCCFG=,,,,1");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    // activate stack
    _at.cmd_start("AT+NETOPEN");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.set_at_timeout(NET_OPEN_CLOSE_TIMEOUT);
    _at.resp_start("+NETOPEN:");
    int open_code = _at.read_int();
    _at.consume_to_stop_tag();
    _at.restore_at_timeout();

    err_1 = _at.get_last_error();
    err_2 = open_code != 0 ? NSAPI_ERROR_NO_CONNECTION : NSAPI_ERROR_OK;
    _at.unlock();
    return any_error(err_1, err_2);
}

nsapi_error_t SIM5320CellularNetwork::disconnect()
{
    nsapi_error_t err;
    _at.lock();
    // deactivate stack
    _at.cmd_start("AT+NETCLOSE");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _at.set_at_timeout(NET_OPEN_CLOSE_TIMEOUT);
    _at.resp_start("+NETCLOSE:");
    int close_code = _at.read_int();
    _at.consume_to_stop_tag();
    _at.restore_at_timeout();
    err = _at.unlock_return_error();
    err = any_error(err, close_code == 0 ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR);
    err = any_error(err, AT_CellularNetwork::disconnect());
    return err;
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
