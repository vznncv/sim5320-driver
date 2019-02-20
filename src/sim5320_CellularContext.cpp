#include "sim5320_CellularContext.h"
#include "sim5320_CellularStack.h"
#include "sim5320_utils.h"
using namespace sim5320;

SIM5320CellularContext::SIM5320CellularContext(ATHandler &at, SIM5320CellularDevice *device, const char *apn, bool cp_req, bool nonip_req)
    : AT_CellularContext(at, device, apn, cp_req, nonip_req)
    , _is_net_opened(false)
    , _sim5320_device(device)
{
    _at.set_urc_handler("+NETOPEN:", callback(this, &SIM5320CellularContext::_urc_netopen));
    _at.set_urc_handler("+NETCLOSE:", callback(this, &SIM5320CellularContext::_urc_netclose));
}

SIM5320CellularContext::~SIM5320CellularContext()
{
    if (_stack) {
        delete _stack;
    }

    _at.set_urc_handler("+NETOPEN:", NULL);
    _at.set_urc_handler("+NETCLOSE:", NULL);
}

static const int PDP_CONTEXT_ID = 1;
static const int PDP_CONTEXT_ACTIVATION_TIMEOUT = 32000;
static const int PDP_CONTEXT_DEACTIVATION_TIMEOUT = 16000;
static const int PDP_STATUS_CHECK_DELAY = 1000;

void SIM5320CellularContext::do_connect()
{
    int err;
    Timer timer;

    call_network_cb(NSAPI_STATUS_CONNECTING);
    if (!_is_context_active) {
        // configure context
        _at.lock();
        // set PDP context parameters
        _at.cmd_start("AT+CGDCONT=");
        _at.write_int(PDP_CONTEXT_ID);
        _at.write_string("IP");
        _at.write_string(_apn);
        _at.cmd_stop_read_resp();
        // set PDP context for sockets
        _at.cmd_start("AT+CSOCKSETPN=");
        _at.write_int(PDP_CONTEXT_ID);
        _at.cmd_stop_read_resp();
        _cid = PDP_CONTEXT_ID;
        // set user/password
        do_user_authentication();
        // check errors
        if ((_cb_data.error = _at.unlock_return_error())) {
            call_network_cb(NSAPI_STATUS_DISCONNECTED);
            return;
        }
        _is_context_active = true;

        // confirm that selected PDP context is active
        call_connection_status_cb(CellularActivatePDPContext);
    }

    // FIXME: should I try to invoke

    // activate network
    // TCP/IP module to use command mode
    _at.lock();
    // set automatic network type selection
    _at.cmd_start("AT+CNMP=");
    _at.write_int(2);
    _at.cmd_stop_read_resp();
    // don't show prompt with remove IP when new data is received
    _at.cmd_start("AT+CIPSRIP=0");
    _at.cmd_stop_read_resp();
    // set command mode (non-transparent mode)
    _at.cmd_start("AT+CIPMODE=0");
    _at.cmd_stop_read_resp();
    // set manual data receive mode
    _at.cmd_start("AT+CIPRXGET=1");
    _at.cmd_stop_read_resp();
    // configure receive urc: "+RECEIVE"
    _at.cmd_start("AT+CIPCCFG=,,,,1");
    _at.cmd_stop_read_resp();
    // activate PDP context
    _at.cmd_start("AT+NETOPEN");
    _at.cmd_stop_read_resp();
    // check errors
    if ((err = _at.unlock_return_error())) {
        call_network_cb(NSAPI_STATUS_DISCONNECTED);
        return;
    };

    // wait network
    timer.start();
    while (!_is_net_opened && timer.read_ms() < PDP_CONTEXT_ACTIVATION_TIMEOUT) {
        wait_ms(PDP_STATUS_CHECK_DELAY);
        _check_netstate();
    }
    if (_is_net_opened) {
        _is_context_activated = true;
        _cb_data.error = NSAPI_ERROR_OK;
        call_network_cb(NSAPI_STATUS_GLOBAL_UP);
    } else {
        _is_context_activated = false;
        _cb_data.error = NSAPI_ERROR_CONNECTION_TIMEOUT;
        call_network_cb(NSAPI_STATUS_DISCONNECTED);
    }
}

#define SIM5320_NETWORK_TIMEOUT 3 * 60 * 1000
#define SIM5320_DEVICE_TIMEOUT 1 * 60 * 1000

uint32_t SIM5320CellularContext::get_timeout_for_operation(CellularContext::ContextOperation op) const
{
    uint32_t timeout = SIM5320_NETWORK_TIMEOUT;
    if (op == OP_SIM_READY || op == OP_DEVICE_READY) {
        timeout = SIM5320_DEVICE_TIMEOUT;
    }
    return timeout;
}

#define CLOSE_NETWORK_ERR_TIMEOUT 500
#define CLOSE_NETWORK_MAX_ATTEMPTS 20

nsapi_error_t SIM5320CellularContext::disconnect()
{
    int err;

    if (!_is_net_opened) {
        return NSAPI_ERROR_OK;
    }

    int close_err_count = 0;
    while (close_err_count < CLOSE_NETWORK_MAX_ATTEMPTS) {
        // try to close network
        _at.lock();
        _at.cmd_start("AT+NETCLOSE");
        _at.cmd_stop_read_resp();
        _at.clear_error();
        _at.unlock();
        // check result
        _check_netstate();
        if (_is_net_opened) {
            close_err_count++;
            wait_ms(CLOSE_NETWORK_ERR_TIMEOUT);
        } else {
            break;
        }
    }
    if (_is_net_opened) {
        return NSAPI_ERROR_TIMEOUT;
    }

    _is_context_activated = false;
    call_network_cb(NSAPI_STATUS_DISCONNECTED);

    return NSAPI_ERROR_OK;
}

bool SIM5320CellularContext::is_connected()
{
    return _is_net_opened;
}

void SIM5320CellularContext::call_connection_status_cb(cellular_connection_status_t status)
{
    if (_status_cb) {
        _status_cb((nsapi_event_t)status, (intptr_t)&_cb_data);
    }
}

nsapi_error_t SIM5320CellularContext::_check_netstate()
{
    _at.lock();
    _at.cmd_start("AT+NETOPEN?");
    _at.cmd_stop();
    _at.resp_start("+NETOPEN:");
    int net_state = _at.read_int();
    _at.skip_param();
    _at.cmd_stop();
    if (!_at.get_last_error()) {
        _is_net_opened = net_state;
    }
    return _at.unlock_return_error();
}

void SIM5320CellularContext::_urc_netopen()
{
    int net_state = _at.read_int();
    if (!_at.get_last_error() && net_state == 0) {
        _is_net_opened = true;
        call_network_cb(NSAPI_STATUS_GLOBAL_UP);
    }
}

void SIM5320CellularContext::_urc_netclose()
{
    int net_state = _at.read_int();
    if (!_at.get_last_error() && net_state == 0) {
        _is_net_opened = false;
        // note: the disconnected
        call_network_cb(NSAPI_STATUS_DISCONNECTED);
    }
}

//nsapi_error_t SIM5320CellularNetwork::init()
//{
//    nsapi_error_t err = AT_CellularNetwork::init();
//    if (err) {
//        return err;
//    }
//    _at.lock();
//    // set automatic selection between GSM and WCDMA
//    // NOTE: the command can fails, so ignore any error
//    int err_code = _at.get_last_error();
//    _at.cmd_start("AT+CNMP=2");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    _at.clear_error();

//    _at.unlock();
//    return err_code;
//}

//#define DEFAULT_PDP_CONTEXT_NO 1
//#define CONTEXT_ACTIVATION_TIMEOUT 16000

//nsapi_error_t SIM5320CellularNetwork::activate_context()
//{
//    nsapi_error_t err;

//    // check that apn is set
//    if (!_apn) {
//        return NSAPI_ERROR_NO_CONNECTION;
//    }

//    // configure context number 1 to use it
//    const char* stack_type;
//    switch (_ip_stack_type_requested) {
//    case IPV4_STACK:
//        stack_type = "IP";
//        break;
//    case IPV6_STACK:
//        stack_type = "IPV6";
//        break;
//    case DEFAULT_STACK:
//        // use IPV4 by default
//        _ip_stack_type_requested = IPV4_STACK;
//        stack_type = "IP";
//        break;
//    default:
//        return NSAPI_ERROR_NO_CONNECTION;
//    }
//    _ip_stack_type = _ip_stack_type_requested;

//    _at.lock();

//    // configure context 1
//    _cid = DEFAULT_PDP_CONTEXT_NO;
//    _new_context_set = false;

//    _at.cmd_start("AT+CGDCONT=");
//    _at.write_int(_cid);
//    _at.write_string(stack_type);
//    _at.write_string(_apn);
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    _at.cmd_start("AT+CGSOCKCONT=");
//    _at.write_int(_cid);
//    _at.write_string(stack_type);
//    _at.write_string(_apn);
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    err = _at.get_last_error();
//    if (err) {
//        return _at.unlock_return_error();
//    }
//    // set user credentials
//    err = do_user_authentication();
//    if (err != NSAPI_ERROR_OK) {
//        return _at.unlock_return_error();
//    }
//    // activate context
//    _at.set_at_timeout(CONTEXT_ACTIVATION_TIMEOUT);
//    _at.cmd_start("AT+CGACT=1,");
//    _at.write_int(_cid);
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    _at.restore_at_timeout();
//    if (!_at.get_last_error()) {
//        _is_context_active = true;
//    }

//    // set active context as default for sockets
//    _at.cmd_start("AT+CSOCKSETPN=");
//    _at.write_int(_cid);
//    _at.cmd_stop();
//    _at.set_at_timeout(CONTEXT_ACTIVATION_TIMEOUT);
//    _at.resp_start();
//    _at.resp_stop();
//    _at.restore_at_timeout();
//    return _at.unlock_return_error();
//}

//#define NET_OPEN_CLOSE_TIMEOUT 30000
//#define NET_OPEN_CLOSE_CHECK_DELAY 5000

//nsapi_error_t SIM5320CellularNetwork::_wait_network_status(int expected_status, int timeout)
//{
//    Timer timer;
//    int status = -1;
//    nsapi_error_t err = _at.get_last_error();
//    if (err) {
//        return err;
//    }

//    timer.start();

//    while (1) {
//        _at.clear_error();
//        _at.cmd_start("AT+NETOPEN?");
//        _at.cmd_stop();
//        _at.resp_start("+NETOPEN: ");
//        status = _at.read_int();
//        _at.resp_stop();
//        if (status == expected_status) {
//            break;
//        }
//        if (timer.read_ms() > NET_OPEN_CLOSE_TIMEOUT) {
//            break;
//            err = NSAPI_ERROR_TIMEOUT;
//        }
//        wait_ms(NET_OPEN_CLOSE_CHECK_DELAY);
//    }
//    return err;
//}

//nsapi_error_t SIM5320CellularNetwork::connect()
//{
//    nsapi_error_t err_1, err_2;
//    err_1 = AT_CellularNetwork::connect();
//    if (err_1) {
//        return err_1;
//    }
//    _at.lock();
//    // don't show prompt with remove IP when new data is received
//    _at.cmd_start("AT+CIPSRIP=0");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    // set command mode (non-transparent mode)
//    _at.cmd_start("AT+CIPMODE=0");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    // set manual data receive mode
//    _at.cmd_start("AT+CIPRXGET=1");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    // configure receive urc: "+RECEIVE"
//    _at.cmd_start("AT+CIPCCFG=,,,,1");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    // activate stack
//    _at.cmd_start("AT+NETOPEN");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    _at.set_at_timeout(NET_OPEN_CLOSE_TIMEOUT);
//    _at.resp_start("+NETOPEN:");
//    int open_code = _at.read_int();
//    _at.consume_to_stop_tag();
//    _at.restore_at_timeout();

//    err_1 = _at.get_last_error();
//    err_2 = open_code != 0 ? NSAPI_ERROR_NO_CONNECTION : NSAPI_ERROR_OK;
//    _at.unlock();
//    return any_error(err_1, err_2);
//}

//nsapi_error_t SIM5320CellularNetwork::disconnect()
//{
//    nsapi_error_t err;
//    _at.lock();
//    // deactivate stack
//    _at.cmd_start("AT+NETCLOSE");
//    _at.cmd_stop();
//    _at.resp_start();
//    _at.resp_stop();
//    _at.set_at_timeout(NET_OPEN_CLOSE_TIMEOUT);
//    _at.resp_start("+NETCLOSE:");
//    int close_code = _at.read_int();
//    _at.consume_to_stop_tag();
//    _at.restore_at_timeout();
//    err = _at.unlock_return_error();
//    err = any_error(err, close_code == 0 ? NSAPI_ERROR_OK : NSAPI_ERROR_DEVICE_ERROR);
//    err = any_error(err, AT_CellularNetwork::disconnect());
//    return err;
//}

NetworkStack *SIM5320CellularContext::get_stack()
{

    if (!_stack) {
        _stack = new SIM5320CellularStack(_at, _cid, (nsapi_ip_stack_t)_pdp_type);
    }
    return _stack;
}

//bool SIM5320CellularNetwork::get_modem_stack_type(nsapi_ip_stack_t requested_stack)
//{
//    switch (requested_stack) {
//    case IPV4_STACK:
//    case IPV6_STACK:
//        return true;
//    default:
//        return false;
//    }
//}

//nsapi_error_t SIM5320CellularNetwork::do_user_authentication()
//{
//    // set username and password
//    if (_pwd && _uname) {
//        // configure CGAUTH
//        _at.cmd_start("AT+CGAUTH=");
//        _at.write_int(_cid);
//        _at.write_int(1);
//        _at.write_string(_pwd);
//        _at.write_string(_uname);
//        _at.cmd_stop();
//        _at.resp_start();
//        _at.resp_stop();
//        // configure SOCKET authentication
//        _at.cmd_start("AT+CSOCKAUTH=");
//        _at.write_int(_cid);
//        _at.write_int(1);
//        _at.write_string(_pwd);
//        _at.write_string(_uname);
//        _at.cmd_stop();
//        _at.resp_start();
//        _at.resp_stop();
//        if (_at.get_last_error() != NSAPI_ERROR_OK) {
//            return NSAPI_ERROR_AUTH_FAILURE;
//        }
//    }
//    return NSAPI_ERROR_OK;
//}
