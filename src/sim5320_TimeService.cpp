#include "sim5320_TimeService.h"
#include "sim5320_utils.h"
#include <stdio.h>
#include <string.h>
using namespace sim5320;

SIM5320TimeService::SIM5320TimeService(ATHandler &at)
    : AT_CellularBase(at)
    , _htp_servers(nullptr)
    , _htp_servers_num(0)
{
}

SIM5320TimeService::~SIM5320TimeService()
{
}

nsapi_error_t SIM5320TimeService::_sync_time_with_htp_servers(const char *const servers[], size_t size)
{
    // note: we explicitly set a list of http server before each update, as after device resetting it loses them.
    int err;
    int res;
    int code;
    const size_t hostname_size = 64;
    char hostname[hostname_size];
    int port;
    int n;

    if (_htp_servers == nullptr) {
        return -1;
    }

    ATHandlerLocker locker(_at);

    // delete old existed domains
    while (!_at.get_last_error()) {
        _at.cmd_start("AT+CHTPSERV=");
        _at.write_string("DEL");
        _at.write_int(0);
        _at.cmd_stop_read_resp();
    }
    // reset error
    _at.clear_error();

    // add servers
    for (size_t i = 0; i < size; i++) {
        n = sscanf(servers[i], "%[^:]:%i", hostname, &port);
        if (n <= 1) {
            port = 80;
        }
        _at.cmd_start("AT+CHTPSERV=");
        _at.write_string("ADD");
        _at.write_string(hostname);
        _at.write_int(port);
        _at.write_int(i);
        _at.cmd_stop_read_resp();
    }

    if ((err = _at.get_last_error())) {
        return err;
    }

    // try to synchronize time with http
    _at.cmd_start("AT+CHTPUPDATE");
    _at.cmd_stop();
    res = read_full_fuzzy_response(_at, true, false, "+CHTPUPDATE", "%i", &code);
    if (res != 1) {
        if (res < 0) {
            return res;
        } else {
            return -1;
        }
    }
    if (code) {
        return code;
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320TimeService::_read_modem_clk(time_t *time)
{
    ATHandlerLocker locker(_at);
    const char timestamp_buf_size = 32;
    int err;
    struct tm tm;
    int n, n_arg, tz, year;
    char timestamp_buf[timestamp_buf_size];
    timestamp_buf[0] = '\0';

    _at.cmd_start("AT+CCLK?");
    _at.cmd_stop();
    _at.resp_start("+CCLK:");
    // expect output: "yy/mm/dd,hh:mm:ss+tz"
    _at.read_string(timestamp_buf, timestamp_buf_size);
    // hack to fix handle bug if `read_string` consumes only "yy/mm/dd" part, but ignore part after a comma
    // see: https://github.com/ARMmbed/mbed-os/issues/12760
    if (strlen(timestamp_buf) == 8) {
        timestamp_buf[8] = ',';
        _at.read_string(timestamp_buf + 9, timestamp_buf_size - 9);
    }
    _at.resp_stop();
    err = _at.get_last_error();
    if (err) {
        return err;
    }

    // parse timestamp
    tz = 0;
    n_arg = sscanf(timestamp_buf, "%i/%i/%i,%i:%i:%2i%i%n", &year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tz, &n);
    if (strlen(timestamp_buf) != n) {
        // fail to parse timestamp
        return -1;
    }
    if (n_arg == 6) {
        tz = 0;
    }

    // adjust month
    tm.tm_mon -= 1;
    // get local time
    *time = mktime(&tm);
    // convert time to UTC
    int tz_seconds = tz * 15 * 60;
    time -= tz_seconds;

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320TimeService::set_htp_servers(const char *const servers[], size_t size)
{
    _htp_servers = servers;
    _htp_servers_num = size;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320TimeService::sync_time()
{
    return _sync_time_with_htp_servers(_htp_servers, _htp_servers_num);
}

nsapi_error_t SIM5320TimeService::get_time(time_t *time)
{
    return _read_modem_clk(time);
}
