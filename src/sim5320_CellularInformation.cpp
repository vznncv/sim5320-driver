#include "sim5320_CellularInformation.h"

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using namespace sim5320;

SIM5320CellularInformation::SIM5320CellularInformation(ATHandler &at_handler)
    : _at(at_handler)
{
}

SIM5320CellularInformation::~SIM5320CellularInformation()
{
}

nsapi_error_t SIM5320CellularInformation::get_manufacturer(char *buf, size_t buf_size)
{
    return _get_simcom_info("+CGMI", "", nullptr, nullptr, buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_model(char *buf, size_t buf_size)
{
    return _get_simcom_info("+CGMM", "", nullptr, nullptr, buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_revision(char *buf, size_t buf_size)
{
    return _get_simcom_info("+CGMR", "", nullptr, "+CGMR:", buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_serial_number(char *buf, size_t buf_size, mbed::CellularInformation::SerialNumberType type)
{
    switch (type) {
    case mbed::CellularInformation::SN:
    case mbed::CellularInformation::IMEI:
        return _get_simcom_info("+CGSN", "", nullptr, nullptr, buf, buf_size);
    default:
        return NSAPI_ERROR_UNSUPPORTED;
    }
}

nsapi_error_t SIM5320CellularInformation::get_imsi(char *imsi, size_t buf_size)
{
    return _get_simcom_info("+CIMI", "", nullptr, nullptr, imsi, buf_size);
}

nsapi_error_t SIM5320CellularInformation::get_iccid(char *buf, size_t buf_size)
{
    return _get_simcom_info("+CICCID", "", nullptr, "+ICCID:", buf, buf_size);
}

nsapi_error_t SIM5320CellularInformation::_get_simcom_info(const char *cmd, const char *cmd_chr, const char *cmd_data, const char *resp_prefix, char *resp_buf, size_t buf_size)
{
    ATHandlerLocker locker(_at);
    if (cmd_data) {
        _at.cmd_start_stop(cmd, cmd_chr, "%b", cmd_data, strlen(cmd_data));
    } else {
        _at.cmd_start_stop(cmd, cmd_chr, "");
    }
    _at.set_delimiter('\r');
    _at.resp_start(resp_prefix);
    resp_buf[0] = '\0';
    _at.read_string(resp_buf, buf_size);
    _at.resp_stop();
    _at.set_default_delimiter();
    return _at.get_last_error();
}
