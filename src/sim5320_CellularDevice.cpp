#include "sim5320_CellularDevice.h"
#include "sim5320_CellularContext.h"
#include "sim5320_CellularInformation.h"
#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularSMS.h"
#include "sim5320_utils.h"
using namespace sim5320;

static const intptr_t cellular_properties[AT_CellularBase::PROPERTY_MAX] = {
    AT_CellularNetwork::RegistrationModeDisable, // C_EREG AT_CellularNetwork::RegistrationMode. What support modem has for this registration type?
    AT_CellularNetwork::RegistrationModeLAC, // C_GREG AT_CellularNetwork::RegistrationMode. What support modem has for this registration type?
    AT_CellularNetwork::RegistrationModeDisable, // C_REG AT_CellularNetwork::RegistrationMode. What support modem has for this registration type?
    0, // 0 = not supported, 1 = supported. AT+CGSN without type is likely always supported similar to AT+GSN.
    1, // 0 = not supported, 1 = supported. Alternative is to support only ATD*99***<cid>#
    1, // 0 = not supported, 1 = supported. APN authentication AT commands supported
    1, // 0 = not supported, 1 = supported. Does modem support IPV4?
    0, // 0 = not supported, 1 = supported. Does modem support IPV6?
    0, // 0 = not supported, 1 = supported. Does modem support dual stack IPV4V6?
    0, // 0 = not supported, 1 = supported. Does modem support Non-IP?
};
// notes:
// 1. TODO: check PPP support
// 2. Enable IPv6, when AT_CellularContext::set_new_context is fixed. (current implementation use dual IPV4V6 stack if IPV4 and IPV6 is supported,
//    instead of IPv4 and IPv6 separately)
// 3. We can choose only one: C_EREG, C_GREG or C_REG otherwise, AT_CellularDevice and CellularStateMachine can cause "reset" action
//    if one of them is registered, but other isn't. As result, ``CellularContext::connect`` doesn't work in this case.

SIM5320CellularDevice::SIM5320CellularDevice(FileHandle *fh)
    : AT_CellularDevice(fh)
{
    set_timeout(SIM5320_DEFAULT_TIMEOUT);
    AT_CellularBase::set_cellular_properties(cellular_properties);
}

SIM5320CellularDevice::~SIM5320CellularDevice()
{
    _gps.cleanup(this);
    _ftp_client.cleanup(this);
    _time_service.cleanup(this);
}

nsapi_error_t SIM5320CellularDevice::init_at_interface()
{
    nsapi_error_t err;
    // disable echo
    ATHandlerLocker locker(*_at);
    _at->flush();
    _at->cmd_start("ATE0"); // echo off
    _at->cmd_stop_read_resp();
    if (_at->get_last_error()) {
        return _at->get_last_error();
    }
    // check AT command response
    if ((err = is_ready())) {
        return err;
    }
    // disable STK function
    _at->cmd_start("AT+STK=0");
    _at->cmd_stop_read_resp();
    return _at->get_last_error();
}

nsapi_error_t SIM5320CellularDevice::set_power_level(int func_level)
{

    // limit support by energy levels 0 an 1
    if (func_level < 0 || func_level > 1) {
        return NSAPI_ERROR_PARAMETER;
    }
    ATHandlerLocker locker(*_at);
    _at->cmd_start("AT+CFUN=");
    _at->write_int(func_level);
    _at->cmd_stop_read_resp();
    return _at->get_last_error();
}

nsapi_error_t SIM5320CellularDevice::get_power_level(int &func_level)
{
    int result;
    ATHandlerLocker locker(*_at);
    _at->cmd_start("AT+CFUN?");
    _at->cmd_stop();
    _at->resp_start("+CFUN:");
    result = _at->read_int();
    _at->resp_stop();
    if (!_at->get_last_error()) {
        func_level = result;
    }
    return _at->get_last_error();
}

void SIM5320CellularDevice::set_timeout(int timeout)
{
    if (timeout < SIM5320_DEFAULT_TIMEOUT) {
        timeout = SIM5320_DEFAULT_TIMEOUT;
    }
    AT_CellularDevice::set_timeout(timeout);
}

nsapi_error_t SIM5320CellularDevice::init()
{
    nsapi_error_t err;
    if ((err = AT_CellularDevice::init())) {
        return err;
    }
    // disable STK function
    ATHandlerLocker locker(*_at);
    _at->cmd_start("AT+STK=0");
    _at->cmd_stop_read_resp();

    //    // switch CMEE codes to string format
    //    _at->cmd_start("AT+CMEE=2"); // verbose responses
    //    _at->cmd_stop_read_resp();

    // disable registration URC codes is they are enabled
    // note: if CellularMachine is used, it will enable them
    _at->cmd_start("AT+CREG=0");
    _at->cmd_stop_read_resp();
    _at->cmd_start("AT+CGREG=0");
    _at->cmd_stop_read_resp();
    return _at->get_last_error();
}

SIM5320GPSDevice *SIM5320CellularDevice::open_gps(FileHandle *fh)
{
    return _gps.open_interface(fh, this);
}

void SIM5320CellularDevice::close_gps()
{
    _gps.close_interface(this);
}

SIM5320FTPClient *SIM5320CellularDevice::open_ftp_client(FileHandle *fh)
{
    return _ftp_client.open_interface(fh, this);
}

void SIM5320CellularDevice::close_ftp_client()
{
    _ftp_client.close_interface(this);
}

SIM5320TimeService *SIM5320CellularDevice::open_time_service(FileHandle *fh)
{
    return _time_service.open_interface(fh, this);
}

void SIM5320CellularDevice::close_time_service()
{
    _time_service.close_interface(this);
}

#define SUBSCRIBER_NUMBER_INDEX 1

nsapi_error_t SIM5320CellularDevice::get_subscriber_number(char *number)
{
    bool find_number = false;
    int number_type;

    ATHandlerLocker locker(*_at);
    // active MSISDN memory
    _at->cmd_start("AT+CPBS=");
    _at->write_string("ON");
    _at->cmd_stop_read_resp();

    _at->cmd_start("AT+CNUM");
    _at->cmd_stop();
    _at->resp_start("+CNUM");
    while (_at->info_resp()) {
        if (find_number) {
            // skip data
            _at->skip_param(3);
        } else {
            // skip alpha
            _at->skip_param();
            // read number
            _at->read_string(number, SUBSCRIBER_NUMBER_MAX_LEN);
            // read type
            number_type = _at->read_int();
            // FIXME: use first valid entity
            find_number = true;
        }
    }

    // CNUM returns nothing
    if (!find_number) {
        number[0] = '\0';
    }

    return _at->get_last_error();
}

nsapi_error_t SIM5320CellularDevice::set_subscriber_number(const char *number)
{
    if (number == NULL) {
        return NSAPI_ERROR_PARAMETER;
    }

    ATHandlerLocker locker(*_at);

    // active MSISDN memory
    _at->cmd_start("AT+CPBS=");
    _at->write_string("ON");
    _at->cmd_stop_read_resp();

    // set subscriber number
    _at->cmd_start("AT+CPBW=");
    _at->write_int(SUBSCRIBER_NUMBER_INDEX);
    _at->write_string(number);
    _at->cmd_stop_read_resp();

    return _at->get_last_error();
}

AT_CellularInformation *SIM5320CellularDevice::open_information_impl(ATHandler &at)
{
    return new SIM5320CellularInformation(at);
}

AT_CellularNetwork *SIM5320CellularDevice::open_network_impl(ATHandler &at)
{
    return new SIM5320CellularNetwork(at);
}

AT_CellularContext *SIM5320CellularDevice::create_context_impl(ATHandler &at, const char *apn, bool cp_req, bool nonip_req)
{
    return new SIM5320CellularContext(at, this, apn, cp_req, nonip_req);
}

#if MBED_CONF_CELLULAR_USE_SMS
AT_CellularSMS *SIM5320CellularDevice::open_sms_impl(ATHandler &at)
{
    return new SIM5320CellularSMS(at);
}
#endif // MBED_CONF_CELLULAR_USE_SMS

SIM5320GPSDevice *SIM5320CellularDevice::open_gps_impl(ATHandler &at)
{
    return new SIM5320GPSDevice(at);
}

SIM5320FTPClient *SIM5320CellularDevice::open_ftp_client_impl(ATHandler &at)
{
    return new SIM5320FTPClient(at);
}

SIM5320TimeService *SIM5320CellularDevice::open_time_service_impl(ATHandler &at)
{
    return new SIM5320TimeService(at);
}
