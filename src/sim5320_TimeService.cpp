#include <stdio.h>
#include <string.h>

#include "sim5320_TimeService.h"

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using namespace sim5320;

SIM5320TimeService::SIM5320TimeService(ATHandler &at)
    : _at(at)
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
        _at.at_cmd_discard("+CHTPSERV", "=", "%s%i", "DEL", 0);
    }
    // reset error
    _at.clear_error();

    // add servers
    for (size_t i = 0; i < size; i++) {
        SimpleStringParser parser(servers[i]);
        parser.consume_string_until_sep(hostname, hostname_size, ':');
        if (parser.is_finshed()) {
            port = 80;
        } else {
            parser.consume_literal(":");
            parser.consume_int(&port);
        }

        if (parser.get_error()) {
            tr_error("Fail to parse http server for time extration");
            continue;
        }

        _at.at_cmd_discard("+CHTPSERV", "=", "%s%s%d%d", "ADD", hostname, port, i);
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
    const size_t timestamp_buf_size = 24;
    int err;
    struct tm tm;
    int n, n_arg, tz;
    int read_len;
    char timestamp_buf[timestamp_buf_size];
    timestamp_buf[0] = '\0';

    _at.cmd_start("AT+CCLK?");
    _at.cmd_stop();
    _at.resp_start("+CCLK:");
    // expect output: "yy/mm/dd,hh:mm:ss+tz"
    read_len = _at.read_string(timestamp_buf, timestamp_buf_size);
    _at.resp_stop();
    err = _at.get_last_error();
    if (err) {
        return err;
    }

    // parse timestamp
    SimpleStringParser parser(timestamp_buf);
    parser.consume_int(&tm.tm_year);
    parser.consume_literal("/");
    parser.consume_int(&tm.tm_mon);
    parser.consume_literal("/");
    parser.consume_int(&tm.tm_mday);
    parser.consume_literal(",");
    parser.consume_int(&tm.tm_hour);
    parser.consume_literal(":");
    parser.consume_int(&tm.tm_min);
    parser.consume_literal(":");
    parser.consume_int(&tm.tm_sec, 2);
    if (parser.is_finshed()) {
        tz = 0;
    } else {
        parser.consume_int(&tz);
    }
    if (parser.get_error()) {
        // fail to parse timestamp
        return parser.get_error();
    }

    // adjust month and year
    tm.tm_mon -= 1;
    tm.tm_year += 100;
    // get local time
    *time = mktime(&tm);
    // convert time to UTC
    int tz_seconds = tz * 15 * 60;
    *time -= tz_seconds;

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320TimeService::set_tzu(bool state)
{
    return at_cmdw_set_b(_at, "+CTZU", state);
}

nsapi_error_t SIM5320TimeService::get_tzu(bool &state)
{
    return at_cmdw_get_b(_at, "+CTZU", state);
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
