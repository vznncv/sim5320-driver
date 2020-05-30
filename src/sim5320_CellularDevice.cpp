#include "sim5320_CellularDevice.h"
#include "sim5320_CellularContext.h"
#include "sim5320_CellularInformation.h"
#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularSMS.h"
#include "sim5320_utils.h"
using namespace sim5320;

static const intptr_t cellular_properties[AT_CellularDevice::PROPERTY_MAX] = {
    AT_CellularNetwork::RegistrationModeDisable, // PROPERTY_C_EREG | AT_CellularNetwork::RegistrationMode. What support modem has for this registration type?
    AT_CellularNetwork::RegistrationModeLAC, // PROPERTY_C_GREG | AT_CellularNetwork::RegistrationMode. What support modem has for this registration type?
    AT_CellularNetwork::RegistrationModeDisable, // PROPERTY_C_REG | AT_CellularNetwork::RegistrationMode. What support modem has for this registration type?

    0, // PROPERTY_AT_CGSN_WITH_TYPE | 0 = not supported, 1 = supported. AT+CGSN without type is likely always supported similar to AT+GSN.
    1, // PROPERTY_AT_CGDATA | 0 = not supported, 1 = supported. Alternative is to support only ATD*99***<cid>#
    1, // PROPERTY_AT_CGAUTH | 0 = not supported, 1 = supported. APN authentication AT commands supported

    1, // PROPERTY_AT_CNMI | 0 = not supported, 1 = supported. New message (SMS) indication AT command
    1, // PROPERTY_AT_CSMP | 0 = not supported, 1 = supported. Set text mode AT command
    1, // PROPERTY_AT_CMGF | 0 = not supported, 1 = supported. Set preferred message format AT command
    1, // PROPERTY_AT_CSDH | 0 = not supported, 1 = supported. Show text mode AT command

    1, // PROPERTY_IPV4_PDP_TYPE | 0 = not supported, 1 = supported. Does modem support IPV4?
    0, // PROPERTY_IPV6_PDP_TYPE | 0 = not supported, 1 = supported. Does modem support IPV6?
    0, // PROPERTY_IPV4V6_PDP_TYPE | 0 = not supported, 1 = supported. Does modem support dual stack IPV4V6?
    0, // PROPERTY_NON_IP_PDP_TYPE | 0 = not supported, 1 = supported. Does modem support Non-IP?

    1, // PROPERTY_AT_CGEREP | 0 = not supported, 1 = supported. Does modem support AT command AT+CGEREP.
    1, // PROPERTY_AT_COPS_FALLBACK_AUTO | 0 = not supported, 1 = supported. Does modem support mode 4 of AT+COPS= ?

    10, // PROPERTY_SOCKET_COUNT | The number of sockets of modem IP stack
    1, // PROPERTY_IP_TCP | 0 = not supported, 1 = supported. Modem IP stack has support for TCP
    1, // PROPERTY_IP_UDP | 0 = not supported, 1 = supported. Modem IP stack has support for TCP

    0, // PROPERTY_AT_SEND_DELAY | Sending delay between AT commands in ms
};

//#if (AT_CellularDevice::PROPERTY_MAX != 3)

//#endif

//PROPERTY_C_EREG,                // AT_CellularNetwork::RegistrationMode. What support modem has for this registration type.
//PROPERTY_C_GREG,                // AT_CellularNetwork::RegistrationMode. What support modem has for this registration type.
//PROPERTY_C_REG,                 // AT_CellularNetwork::RegistrationMode. What support modem has for this registration type.
//PROPERTY_AT_CGSN_WITH_TYPE,     // 0 = not supported, 1 = supported. AT+CGSN without type is likely always supported similar to AT+GSN.
//PROPERTY_AT_CGDATA,             // 0 = not supported, 1 = supported. Alternative is to support only ATD*99***<cid>#
//PROPERTY_AT_CGAUTH,             // 0 = not supported, 1 = supported. APN authentication AT commands supported
//PROPERTY_AT_CNMI,               // 0 = not supported, 1 = supported. New message (SMS) indication AT command
//PROPERTY_AT_CSMP,               // 0 = not supported, 1 = supported. Set text mode AT command
//PROPERTY_AT_CMGF,               // 0 = not supported, 1 = supported. Set preferred message format AT command
//PROPERTY_AT_CSDH,               // 0 = not supported, 1 = supported. Show text mode AT command
//PROPERTY_IPV4_PDP_TYPE,         // 0 = not supported, 1 = supported. Does modem support IPV4?
//PROPERTY_IPV6_PDP_TYPE,         // 0 = not supported, 1 = supported. Does modem support IPV6?
//PROPERTY_IPV4V6_PDP_TYPE,       // 0 = not supported, 1 = supported. Does modem support IPV4 and IPV6 simultaneously?
//PROPERTY_NON_IP_PDP_TYPE,       // 0 = not supported, 1 = supported. Does modem support Non-IP?
//PROPERTY_AT_CGEREP,             // 0 = not supported, 1 = supported. Does modem support AT command AT+CGEREP.
//PROPERTY_AT_COPS_FALLBACK_AUTO, // 0 = not supported, 1 = supported. Does modem support mode 4 of AT+COPS= ?
//PROPERTY_SOCKET_COUNT,          // The number of sockets of modem IP stack
//PROPERTY_IP_TCP,                // 0 = not supported, 1 = supported. Modem IP stack has support for TCP
//PROPERTY_IP_UDP,                // 0 = not supported, 1 = supported. Modem IP stack has support for TCP
//PROPERTY_AT_SEND_DELAY,         // Sending delay between AT commands in ms
//PROPERTY_MAX

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

    MBED_STATIC_ASSERT(AT_CellularDevice::PROPERTY_MAX == 20, "Wrong number of cellular property. Please check and fix driver implementation");
    AT_CellularDevice::set_cellular_properties(cellular_properties);
}

SIM5320CellularDevice::~SIM5320CellularDevice()
{
    _location_service.cleanup(this);
    _ftp_client.cleanup(this);
    _time_service.cleanup(this);
}

nsapi_error_t SIM5320CellularDevice::init_at_interface()
{
    nsapi_error_t err;
    // disable echo
    ATHandlerLocker locker(_at);
    _at.flush();
    _at.cmd_start("ATE0"); // echo off
    _at.cmd_stop_read_resp();
    if (_at.get_last_error()) {
        return _at.get_last_error();
    }
    // check AT command response
    if ((err = is_ready())) {
        return err;
    }
    // disable STK function
    _at.cmd_start("AT+STK=0");
    _at.cmd_stop_read_resp();

    // configure handlers at initialization step to prevent memory
    // allocation during modem usage
    // note: if AT_CellularDevice::setup_at_handler is dropped or changed,
    // this invocation can be removed
    setup_at_handler();

    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularDevice::set_power_level(int func_level)
{

    // limit support by energy levels 0 an 1
    if (func_level < 0 || func_level > 1) {
        return NSAPI_ERROR_PARAMETER;
    }
    ATHandlerLocker locker(_at);
    _at.cmd_start("AT+CFUN=");
    _at.write_int(func_level);
    _at.cmd_stop_read_resp();
    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularDevice::get_power_level(int &func_level)
{
    int result;
    ATHandlerLocker locker(_at);
    _at.cmd_start("AT+CFUN?");
    _at.cmd_stop();
    _at.resp_start("+CFUN:");
    result = _at.read_int();
    _at.resp_stop();
    if (!_at.get_last_error()) {
        func_level = result;
    }
    return _at.get_last_error();
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
    ATHandlerLocker locker(_at);
    _at.cmd_start("AT+STK=0");
    _at.cmd_stop_read_resp();

    //    // switch CMEE codes to string format
    //    _at->cmd_start("AT+CMEE=2"); // verbose responses
    //    _at->cmd_stop_read_resp();

    // disable registration URC codes is they are enabled
    // note: if CellularMachine is used, it will enable them
    _at.cmd_start("AT+CREG=0");
    _at.cmd_stop_read_resp();
    _at.cmd_start("AT+CGREG=0");
    _at.cmd_stop_read_resp();
    err = _at.get_last_error();

    // set automatic radio access technology selection
    _at.at_cmd_discard("+CNMP", "=", "%i", 2);

    return err;
}

SIM5320LocationService *SIM5320CellularDevice::open_location_service()
{
    return _location_service.open_interface(this);
}

void SIM5320CellularDevice::close_location_service()
{
    _location_service.close_interface(this);
}

SIM5320FTPClient *SIM5320CellularDevice::open_ftp_client()
{
    return _ftp_client.open_interface(this);
}

void SIM5320CellularDevice::close_ftp_client()
{
    _ftp_client.close_interface(this);
}

SIM5320TimeService *SIM5320CellularDevice::open_time_service()
{
    return _time_service.open_interface(this);
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

    ATHandlerLocker locker(_at);
    // active MSISDN memory
    _at.cmd_start("AT+CPBS=");
    _at.write_string("ON");
    _at.cmd_stop_read_resp();

    _at.cmd_start("AT+CNUM");
    _at.cmd_stop();
    _at.resp_start("+CNUM");
    while (_at.info_resp()) {
        if (find_number) {
            // skip data
            _at.skip_param(3);
        } else {
            // skip alpha
            _at.skip_param();
            // read number
            _at.read_string(number, SUBSCRIBER_NUMBER_MAX_LEN);
            // read type
            number_type = _at.read_int();
            // FIXME: use first valid entity
            find_number = true;
        }
    }

    // CNUM returns nothing
    if (!find_number) {
        number[0] = '\0';
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularDevice::set_subscriber_number(const char *number)
{
    if (number == NULL) {
        return NSAPI_ERROR_PARAMETER;
    }

    ATHandlerLocker locker(_at);

    // active MSISDN memory
    _at.cmd_start("AT+CPBS=");
    _at.write_string("ON");
    _at.cmd_stop_read_resp();

    // set subscriber number
    _at.cmd_start("AT+CPBW=");
    _at.write_int(SUBSCRIBER_NUMBER_INDEX);
    _at.write_string(number);
    _at.cmd_stop_read_resp();

    return _at.get_last_error();
}

AT_CellularInformation *SIM5320CellularDevice::open_information_impl(ATHandler &at)
{
    return new SIM5320CellularInformation(at, *this);
}

AT_CellularNetwork *SIM5320CellularDevice::open_network_impl(ATHandler &at)
{
    return new SIM5320CellularNetwork(at, *this);
}

AT_CellularContext *SIM5320CellularDevice::create_context_impl(ATHandler &at, const char *apn, bool cp_req, bool nonip_req)
{
    return new SIM5320CellularContext(at, this, apn, cp_req, nonip_req);
}

SIM5320LocationService *SIM5320CellularDevice::open_location_service_impl(ATHandler &at)
{
    return new SIM5320LocationService(at, *this);
}

#if MBED_CONF_CELLULAR_USE_SMS
AT_CellularSMS *SIM5320CellularDevice::open_sms_impl(ATHandler &at)
{
    return new SIM5320CellularSMS(at, *this);
}
#endif // MBED_CONF_CELLULAR_USE_SMS

SIM5320FTPClient *SIM5320CellularDevice::open_ftp_client_impl(ATHandler &at)
{
    return new SIM5320FTPClient(at, *this);
}

SIM5320TimeService *SIM5320CellularDevice::open_time_service_impl(ATHandler &at)
{
    return new SIM5320TimeService(at, *this);
}
