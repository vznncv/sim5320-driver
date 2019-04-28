#include "sim5320_driver.h"
#include "sim5320_CellularNetwork.h"
#include "sim5320_utils.h"
using namespace sim5320;

static const int SIM5320_SERIAL_BAUDRATE = 115200;

SIM5320::SIM5320(UARTSerial *serial_ptr, PinName rts, PinName cts, PinName rst)
    : _rts(rts)
    , _cts(cts)
    , _serial_ptr(serial_ptr)
    , _cleanup_uart(false)
    , _rst(rst)
{
    _init_driver();
}

SIM5320::SIM5320(PinName tx, PinName rx, PinName rts, PinName cts, PinName rst)
    : _rts(rts)
    , _cts(cts)
    , _serial_ptr(new UARTSerial(tx, rx))
    , _cleanup_uart(true)
    , _rst(rst)
{
    _init_driver();
}

void SIM5320::_init_driver()
{
    // configure serial parameters
    _serial_ptr->set_baud(SIM5320_SERIAL_BAUDRATE);
    _serial_ptr->set_format(8, UARTSerial::None, 1);

    // configure hardware reset pin
    if (_rst != NC) {
        _rst_out_ptr = new DigitalOut(_rst, 1);
    } else {
        _rst_out_ptr = NULL;
    }

    // create driver interface
    _device = new SIM5320CellularDevice(_serial_ptr);
    _information = _device->open_information(_serial_ptr);
    _network = _device->open_network(_serial_ptr);
    _sms = _device->open_sms(_serial_ptr);
    _context = _device->create_context(_serial_ptr);
    _gps = _device->open_gps(_serial_ptr);
    _ftp_client = _device->open_ftp_client(_serial_ptr);

    _startup_request_count = 0;
    _at = _device->get_at_handler(_serial_ptr);
}

SIM5320::~SIM5320()
{
    _device->close_information();
    _device->close_network();
    _device->close_sms();
    _device->delete_context(_context);
    _device->close_gps();
    _device->release_at_handler(_at);
    _device->close_ftp_client();
    delete _device;

    if (_rst_out_ptr) {
        delete _rst_out_ptr;
    }

    if (_cleanup_uart) {
        delete _serial_ptr;
    }
}

nsapi_error_t SIM5320::start_uart_hw_flow_ctrl()
{
    _at->lock();
    if (_rts != NC && _cts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::RTSCTS, _rts, _cts);
        _at->cmd_start("AT+IFC=2,2");
        _at->cmd_stop();
    } else if (_rts != NC) {
        _at->cmd_start("AT+IFC=2,0");
        _at->cmd_stop();
    } else if (_cts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::CTS, _cts);
        _at->cmd_start("AT+IFC=0,2");
        _at->cmd_stop();
    } else {
        _at->unlock();
        return NSAPI_ERROR_PARAMETER;
    }
    _at->resp_start();
    _at->resp_stop();
    return _at->unlock_return_error();
}

nsapi_error_t SIM5320::stop_uart_hw_flow_ctrl()
{
    _at->lock();
    if (_rts != NC || _cts != NC) {
        _serial_ptr->set_flow_control(SerialBase::Disabled, _rts, _cts);
        _at->cmd_start("AT+IFC=0,0");
        _at->cmd_stop_read_resp();
    }
    return _at->unlock_return_error();
}

nsapi_error_t SIM5320::init()
{
    nsapi_error_t err;
    // check that UART works and perform basic uart configuration
    if ((err = _device->init_at_interface())) {
        return err;
    }

    // force power mode to 0
    if ((err = _device->set_power_level(0))) {
        return err;
    }

    //    // set automatic radio access technology choosing
    //    SIM5320CellularNetwork *sim5320_network = (SIM5320CellularNetwork *)_network;
    //    if ((err = sim5320_network->set_preffered_radio_access_technology_mode(SIM5320CellularNetwork::PRA;TM_AUTOMATIC))) {
    //        return err;
    //    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320::set_factory_settings()
{
    _at->lock();
    _at->cmd_start("AT&F");
    _at->cmd_stop_read_resp();
    _at->cmd_start("AT&F1");
    _at->cmd_stop_read_resp();
    return _at->unlock_return_error();
}

nsapi_error_t SIM5320::request_to_start()
{
    nsapi_error_t err = NSAPI_ERROR_OK;
    if (_startup_request_count == 0) {
        err = _device->init();
    }
    _startup_request_count++;
    return err;
}

nsapi_error_t SIM5320::request_to_stop()
{
    nsapi_error_t err = NSAPI_ERROR_OK;
    if (_startup_request_count <= 0) {
        // invalid count
        return -1;
    }
    _startup_request_count--;
    if (_startup_request_count == 0) {
        err = _device->shutdown();
    }
    return err;
}

nsapi_error_t SIM5320::process_urc()
{
    _at->process_oob();
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320::reset(SIM5320::ResetMode reset_mode)
{
    int err;
    int func_level;
    if ((err = _device->get_power_level(func_level))) {
        func_level = 0;
    }

    switch (reset_mode) {
    case sim5320::SIM5320::RESET_MODE_DEFAULT:
        err = _reset_soft();
        if (err) {
            err = _reset_hard();
        }
        break;
    case sim5320::SIM5320::RESET_MODE_SOFT:
        err = _reset_soft();
        break;
    case sim5320::SIM5320::RESET_MODE_HARD:
        err = _reset_hard();
        break;
    }
    if (err) {
        return err;
    }

    err = _device->init_at_interface();
    if (err) {
        return err;
    }

    err = _device->set_power_level(func_level);
    if (err) {
        return err;
    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320::is_active(bool &active)
{
    int err;
    int func_level;
    if ((err = _device->get_power_level(func_level))) {
        return err;
    }

    active = func_level != 0;
    return NSAPI_ERROR_OK;
}

CellularDevice *SIM5320::get_device()
{
    return _device;
}

CellularInformation *SIM5320::get_information()
{
    return _information;
}

CellularNetwork *SIM5320::get_network()
{
    return _network;
}

CellularSMS *SIM5320::get_sms()
{
    return _sms;
}

CellularContext *SIM5320::get_context()
{
    return _context;
}

SIM5320GPSDevice *SIM5320::get_gps()
{
    return _gps;
}

SIM5320FTPClient *SIM5320::get_ftp_client()
{
    return _ftp_client;
}

nsapi_error_t SIM5320::_reset_soft()
{
    _at->lock();
    // send reset command
    _at->cmd_start("AT+CRESET");
    _at->cmd_stop_read_resp();

    // if error occurs then stop action
    SIM5320_UNLOCK_RETURN_IF_ERROR(_at);
    _at->unlock();

    // wait device startup messages
    return _skip_initialization_messages();
}

nsapi_error_t SIM5320::_reset_hard()
{
    if (_rst_out_ptr) {
        // try using hardware pin
        _rst_out_ptr->write(0);
        wait_ms(100);
        _rst_out_ptr->write(1);
        // wait startup
        wait_ms(200);
        _at->flush();
        _at->clear_error();
        return _skip_initialization_messages();
    } else {
        return -1;
    }
}

nsapi_error_t SIM5320::_skip_initialization_messages()
{
    int res;

    _at->lock();
    _at->set_at_timeout(_STARTUP_TIMEOUT_MS);
    _at->resp_start("START", true);
    res = _at->get_last_error();
    // if there is not error, wait PB DONE
    _at->resp_start("PB DONE", true);
    // clear any error codes of this step
    _at->clear_error();
    _at->restore_at_timeout();
    _at->unlock();

    return res;
}
