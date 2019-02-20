#include "sim5320_CellularInformation.h"
using namespace sim5320;

SIM5320CellularInformation::SIM5320CellularInformation(ATHandler &at_handler)
    : AT_CellularInformation(at_handler)
{
}

SIM5320CellularInformation::~SIM5320CellularInformation()
{
}

nsapi_error_t SIM5320CellularInformation::get_manufacturer(char *buf, size_t buf_size)
{
    return _get_simcom_info("AT+CGMI", NULL, buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_model(char *buf, size_t buf_size)
{
    return _get_simcom_info("AT+CGMM", NULL, buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_revision(char *buf, size_t buf_size)
{
    return _get_simcom_info("AT+CGMR", "+CGMR:", buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_serial_number(char *buf, size_t buf_size, mbed::CellularInformation::SerialNumberType type)
{
    switch (type) {
    case mbed::CellularInformation::SN:
    case mbed::CellularInformation::IMEI:
        return _get_simcom_info("AT+CGSN", NULL, buf, buf_size);

    case mbed::CellularInformation::IMEISV:
    case mbed::CellularInformation::SVN:
        return NSAPI_ERROR_UNSUPPORTED;
    }
}

nsapi_error_t SIM5320CellularInformation::get_imsi(char *imsi, size_t buf_size)
{
    return _get_simcom_info("AT+CIMI", NULL, imsi, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_iccid(char *buf, size_t buf_size)
{
    return _get_simcom_info("AT+CICCID", "+ICCID:", buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::_get_simcom_info(const char *cmd, const char *response_prefix, char *buf, size_t buf_size)
{

    if (buf == NULL || buf_size == 0) {
        return NSAPI_ERROR_PARAMETER;
    }
    _at.lock();
    _at.cmd_start(cmd);
    _at.cmd_stop();
    _at.set_delimiter('\r');
    _at.resp_start(response_prefix);
    _at.read_string(buf, buf_size - 1);
    _at.resp_stop();
    _at.set_default_delimiter();
    return _at.unlock_return_error();
}
