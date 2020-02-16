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
        {
            // configure context
            ATHandlerLocker locker(_at);
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
            _cb_data.error = _at.get_last_error();
        }
        if (_cb_data.error) {
            call_network_cb(NSAPI_STATUS_DISCONNECTED);
            return;
        }
        _is_context_active = true;

        // confirm that selected PDP context is active
        call_connection_status_cb(CellularActivatePDPContext);
    }

    // activate network
    {
        // TCP/IP module to use command mode
        ATHandlerLocker locker(_at);
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
        err = _at.get_last_error();
    }
    // check errors
    if (err) {
        call_network_cb(NSAPI_STATUS_DISCONNECTED);
        return;
    };

    // wait network
    timer.start();
    while (!_is_net_opened && timer.read_ms() < PDP_CONTEXT_ACTIVATION_TIMEOUT) {
        ThisThread::sleep_for(PDP_STATUS_CHECK_DELAY);
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
    if (!_is_net_opened) {
        return NSAPI_ERROR_OK;
    }

    int close_err_count = 0;
    while (close_err_count < CLOSE_NETWORK_MAX_ATTEMPTS) {
        {
            // try to close network
            ATHandlerLocker locker(_at);
            _at.cmd_start("AT+NETCLOSE");
            _at.cmd_stop_read_resp();
            _at.clear_error();

            // check result
            _check_netstate();
        }
        if (_is_net_opened) {
            close_err_count++;
            ThisThread::sleep_for(CLOSE_NETWORK_ERR_TIMEOUT);
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
    ATHandlerLocker locker(_at);
    _at.cmd_start("AT+NETOPEN?");
    _at.cmd_stop();
    _at.resp_start("+NETOPEN:");
    int net_state = _at.read_int();
    _at.skip_param();
    _at.cmd_stop();
    if (!_at.get_last_error()) {
        _is_net_opened = net_state;
    }
    return _at.get_last_error();
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
        call_network_cb(NSAPI_STATUS_DISCONNECTED);
    }
}

NetworkStack *SIM5320CellularContext::get_stack()
{

    if (!_stack) {
        _stack = new SIM5320CellularStack(_at, _cid, (nsapi_ip_stack_t)_pdp_type);
    }
    return _stack;
}
