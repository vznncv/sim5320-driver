#include "sim5320_CellularDevice.h"
#include "sim5320_CellularContext.h"
#include "sim5320_CellularInformation.h"
#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularSMS.h"
#include "sim5320_utils.h"
using namespace sim5320;

#define SIM5320_DEFAULT_TIMEOUT 8000

static const intptr_t cellular_properties[AT_CellularBase::PROPERTY_MAX] = {
    AT_CellularNetwork::RegistrationModeDisable, // C_EREG AT_CellularNetwork::RegistrationMode. What support modem has for this registration type.
    AT_CellularNetwork::RegistrationModeLAC, // C_GREG AT_CellularNetwork::RegistrationMode. What support modem has for this registration type.
    AT_CellularNetwork::RegistrationModeDisable, // C_REG AT_CellularNetwork::RegistrationMode. What support modem has for this registration type.
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
//    but IPV4V6 isn't)
// 3. We can choose only one: C_EREG, C_GREG or C_REG otherwise, AT_CellularDevice and CellularStateMachine can cause "reset" action
//    if one of them is registered, but other isn't. As result, ``CellularContext::connect`` won't work.

SIM5320CellularDevice::SIM5320CellularDevice(FileHandle *fh)
    : AT_CellularDevice(fh)
    , _gps_ref_count(0)
    , _ftp_client_ref_count(0)
    , _gps(NULL)
    , _ftp_client(NULL)
{
    set_timeout(SIM5320_DEFAULT_TIMEOUT);
    AT_CellularBase::set_cellular_properties(cellular_properties);
}

SIM5320CellularDevice::~SIM5320CellularDevice()
{
}

nsapi_error_t SIM5320CellularDevice::init_at_interface()
{
    nsapi_error_t err;
    // disable echo
    _at->lock();
    _at->flush();
    _at->cmd_start("ATE0"); // echo off
    _at->cmd_stop_read_resp();
    SIM5320_UNLOCK_RETURN_IF_ERROR(_at);
    // check AT command response
    if ((err = is_ready())) {
        _at->unlock();
        return err;
    }
    // disable STK function
    _at->cmd_start("AT+STK=0");
    _at->cmd_stop_read_resp();
    return _at->unlock_return_error();
}

nsapi_error_t SIM5320CellularDevice::set_power_level(int func_level)
{

    // limit support by energy levels 0 an 1
    if (func_level < 0 || func_level > 1) {
        return NSAPI_ERROR_PARAMETER;
    }
    _at->lock();
    _at->cmd_start("AT+CFUN=");
    _at->write_int(func_level);
    _at->cmd_stop_read_resp();
    return _at->unlock_return_error();
}

nsapi_error_t SIM5320CellularDevice::get_power_level(int &func_level)
{
    int result;
    _at->lock();
    _at->cmd_start("AT+CFUN?");
    _at->cmd_stop();
    _at->resp_start("+CFUN:");
    result = _at->read_int();
    _at->resp_stop();
    if (!_at->get_last_error()) {
        func_level = result;
    }
    return _at->unlock_return_error();
}

nsapi_error_t SIM5320CellularDevice::init()
{
    nsapi_error_t err;
    if ((err = AT_CellularDevice::init())) {
        return err;
    }
    // disable STK function
    _at->lock();
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
    return _at->unlock_return_error();
}

SIM5320GPSDevice *SIM5320CellularDevice::open_gps(FileHandle *fh)
{
    if (!_gps) {
        _gps = open_gps_impl(*get_at_handler(fh));
    }
    _gps_ref_count++;
    return _gps;
}

void SIM5320CellularDevice::close_gps()
{
    if (_gps) {
        _gps_ref_count--;
        if (_gps_ref_count == 0) {
            ATHandler *atHandler = &_gps->get_at_handler();
            delete _gps;
            _gps = NULL;
            release_at_handler(atHandler);
        }
    }
}

SIM5320FTPClient *SIM5320CellularDevice::open_ftp_client(FileHandle *fh)
{
    if (!_ftp_client) {
        _ftp_client = open_ftp_client_impl(*get_at_handler(fh));
    }
    _ftp_client_ref_count++;
    return _ftp_client;
}

void SIM5320CellularDevice::close_ftp_client()
{
    if (_ftp_client) {
        _ftp_client_ref_count--;
        if (_ftp_client_ref_count == 0) {
            ATHandler *atHandler = &_ftp_client->get_at_handler();
            delete _ftp_client;
            _ftp_client = NULL;
            release_at_handler(atHandler);
        }
    }
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

AT_CellularSMS *SIM5320CellularDevice::open_sms_impl(ATHandler &at)
{
    //return new SIM5320CellularSMS(at);
    return NULL;
}

SIM5320GPSDevice *SIM5320CellularDevice::open_gps_impl(ATHandler &at)
{
    return new SIM5320GPSDevice(at);
}

SIM5320FTPClient *SIM5320CellularDevice::open_ftp_client_impl(ATHandler &at)
{
    return new SIM5320FTPClient(at);
}
