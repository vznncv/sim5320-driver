#include "sim5320_driver.h"

using namespace sim5320;

static const int SIM5320_SERIAL_BAUDRATE = 115200;

SIM5320::SIM5320(UARTSerial* serial_ptr, PinName rts, PinName cts, PinName rst)
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
    _device = new SIM5320CellularDevice(*mbed_event_queue());
    _power = _device->open_power(_serial_ptr);
    _information = _device->open_information(_serial_ptr);
    _network = _device->open_network(_serial_ptr);
    _sim = _device->open_sim(_serial_ptr);
    _sms = _device->open_sms(_serial_ptr);
    _gps = _device->open_gps(_serial_ptr);
    _ftp_client = _device->open_ftp_client(_serial_ptr);

    _startup_request_count = 0;
    _at_ptr = _device->get_at_handler(_serial_ptr);
}

SIM5320::~SIM5320()
{
    if (_cleanup_uart) {
        delete _serial_ptr;
    }
    _device->close_power();
    _device->close_information();
    _device->close_network();
    _device->close_sim();
    _device->close_gps();
    _device->release_at_handler(_at_ptr);
    _device->close_ftp_client();
    delete _device;

    if (_rst_out_ptr) {
        delete _rst_out_ptr;
    }
}

nsapi_error_t SIM5320::start_uart_hw_flow_ctrl()
{
    _at_ptr->lock();
    if (_rts != NC && _cts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::RTSCTS, _rts, _cts);
        _at_ptr->cmd_start("AT+IFC=2,2");
        _at_ptr->cmd_stop();
    } else if (_rts != NC) {
        _at_ptr->cmd_start("AT+IFC=2,0");
        _at_ptr->cmd_stop();
    } else if (_cts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::CTS, _cts);
        _at_ptr->cmd_start("AT+IFC=0,2");
        _at_ptr->cmd_stop();
    } else {
        _at_ptr->unlock();
        return NSAPI_ERROR_PARAMETER;
    }
    _at_ptr->resp_start();
    _at_ptr->resp_stop();
    return _at_ptr->unlock_return_error();
}

nsapi_error_t SIM5320::stop_uart_hw_flow_ctrl()
{
    _at_ptr->lock();

    if (_rts != NC || _cts != NC) {
        _serial_ptr->set_flow_control(SerialBase::Disabled, _rts, _cts);
        _at_ptr->cmd_start("AT+IFC=0,0");
        _at_ptr->cmd_stop();
        _at_ptr->resp_start();
        _at_ptr->resp_stop();
    }

    return _at_ptr->unlock_return_error();
}

nsapi_error_t SIM5320::init()
{
    nsapi_error_t err;
    // devices initialization
    err = _device->init_module(_serial_ptr);
    if (err) {
        return err;
    }
    // configure at mode
    err = _power->set_at_mode();
    if (err) {
        return err;
    }
    // set GPS settings
    err = _gps->init();
    if (err) {
        return err;
    }
    // configure network driver
    err = _network->init();
    if (err) {
        return err;
    }
    // configure SMS to text mode
    err = _sms->initialize(CellularSMS::CellularSMSMmodeText);
    if (err) {
        return err;
    }
    // prefer to store all data in the sim
    err = _sms->set_cpms("SM", "SM", "SM");
    if (err) {
        return err;
    }
    // switch device into low power mode
    err = _power->set_power_level(0);
    return err;
}

nsapi_error_t SIM5320::reset()
{
    nsapi_error_t err;
    // reset module
    err = _power->reset();
    if (err) {
        if (_rst_out_ptr) {
            // try using hardware pin
            _rst_out_ptr->write(0);
            wait_ms(100);
            _rst_out_ptr->write(1);
            // wait startup
            wait_ms(10000);
            _at_ptr->flush();
            _at_ptr->clear_error();
            err = 0;
        } else {
            return err;
        }
    }
    // switch device into low power mode
    err = _power->set_power_level(0);
    return err;
}

nsapi_error_t SIM5320::set_factory_settings()
{
    _at_ptr->lock();
    _at_ptr->cmd_start("AT&F");
    _at_ptr->cmd_stop();
    _at_ptr->resp_start();
    _at_ptr->resp_stop();
    _at_ptr->cmd_start("AT&F1");
    _at_ptr->cmd_stop();
    _at_ptr->resp_start();
    _at_ptr->resp_stop();
    return _at_ptr->unlock_return_error();
}

nsapi_error_t SIM5320::request_to_start()
{
    nsapi_error_t err = NSAPI_ERROR_OK;
    if (_startup_request_count == 0) {
        err = _power->set_power_level(1);
        if (err) {
            return err;
        }
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
        err = _power->set_power_level(0);
    }
    return err;
}

#define REGISTRATION_STATUS_DELAY 1000

nsapi_error_t SIM5320::wait_network_registration(int timeout_ms)
{
    Timer timer;
    nsapi_error_t err_code;
    CellularNetwork::RegistrationStatus registration_status;

    timer.start();
    while (1) {
        err_code = _network->get_registration_status(CellularNetwork::C_REG, registration_status);
        if (err_code) {
            break;
        }
        if (registration_status == CellularNetwork::RegisteredHomeNetwork || registration_status == CellularNetwork::RegisteredRoaming) {
            break;
        }
        if (timer.read_ms() > timeout_ms) {
            err_code = NSAPI_ERROR_TIMEOUT;
            break;
        }

        wait_ms(REGISTRATION_STATUS_DELAY);
    }

    return err_code;
}

nsapi_error_t SIM5320::is_active(bool& active)
{
    int power_mode;
    _at_ptr->lock();
    _at_ptr->cmd_start("AT+CFUN?");
    _at_ptr->cmd_stop();
    _at_ptr->resp_start("+CFUN:");
    power_mode = _at_ptr->read_int();
    _at_ptr->resp_stop();
    active = power_mode != 0;
    return _at_ptr->unlock_return_error();
}

CellularPower* SIM5320::get_power()
{
    return _power;
}

CellularInformation* SIM5320::get_information()
{
    return _information;
}

CellularNetwork* SIM5320::get_network()
{
    return _network;
}

CellularSIM* SIM5320::get_sim()
{
    return _sim;
}

CellularSMS* SIM5320::get_sms()
{
    return _sms;
}

SIM5320GPSDevice* SIM5320::get_gps()
{
    return _gps;
}

SIM5320FTPClient* SIM5320::get_ftp_client()
{
    return _ftp_client;
}
