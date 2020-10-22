#include "sim5320_LocationService.h"

#include <chrono>

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using mbed::chrono::milliseconds_u32;
using namespace sim5320;

#define to_ms_u32(value) std::chrono::duration_cast<milliseconds_u32>(value)

void SIM5320LocationService::_cgpsftm_urc()
{
    const size_t data_buf_size = 8;
    char data_buf[data_buf_size];
    ssize_t res;
    int total_sat = 0;

    _last_cgpsftm_urc_timestamp = to_ms_u32(_up_timer.elapsed_time());

    // skip first comma after "$GPGSV"
    _at.skip_param();

    while (true) {
        // read satellite in view (SV) code
        res = _at.read_string(data_buf, data_buf_size);
        if (res < 0) {
            break;
        }
        // read satellite sinal power
        res = _at.read_string(data_buf, data_buf_size);
        if (res < 0) {
            break;
        }
        // satellite is found
        total_sat++;
    }

    _last_cgpsftm_urc_sats = total_sat;
}

SIM5320LocationService::SIM5320LocationService(ATHandler &at)
    : _at(at)
{
    _at.set_urc_handler("$GPGSV", callback(this, &SIM5320LocationService::_cgpsftm_urc));
}

SIM5320LocationService::~SIM5320LocationService()
{
    _at.set_urc_handler("$GPGSV", nullptr);
}

nsapi_error_t SIM5320LocationService::init()
{
    ATHandlerLocker locker(_at);

    // disable automatic (AT+CGPSAUTO) GPS start
    at_cmdw_set_i(_at, "+CGPSAUTO", 0, false);
    // set position mode (AT+CGPSPMD) to 127
    at_cmdw_set_i(_at, "+CGPSPMD", 127, false);
    // ensure that GPS debug mode is disabled
    _at.at_cmd_discard("+CGPSFTM", "=", "%d", 0);

    return _at.get_last_error();
}

//****
// GPS receivers timeout constants declaration (it isn't necessary since C++17)
//****
constexpr milliseconds_u32 SIM5320LocationService::_GPS_START_TIMEOUT;
constexpr milliseconds_u32 SIM5320LocationService::_GPS_STOP_TIMEOUT;
constexpr milliseconds_u32 SIM5320LocationService::_GPS_SS_CHECK_PERIOD;

nsapi_error_t SIM5320LocationService::_wait_gps_start_stop(bool state, milliseconds_u32 timeout, milliseconds_u32 check_period)
{
    milliseconds_u32 start_time = to_ms_u32(_up_timer.elapsed_time());
    milliseconds_u32 elapsed_time;
    ATHandlerLocker locker(_at, timeout + 1000ms);

    bool active = !state;
    int err;

    _up_timer.elapsed_time();

    while (true) {
        elapsed_time = to_ms_u32(_up_timer.elapsed_time()) - start_time;
        err = gps_is_active(active);
        if (err) {
            return err;
        }
        if (state == active) {
            break;
        }
        if (elapsed_time > timeout) {
            break;
        }
        ThisThread::sleep_for(check_period);
    }
    if (state != active) {
        return MBED_ERROR_CODE_TIME_OUT;
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320LocationService::gps_start(SIM5320LocationService::GPSMode mode, SIM5320LocationService::GPSStartupMode startup_mode)
{
    ATHandlerLocker locker(_at);
    int err;

    if (mode == GPS_MODE_STANDALONE) {
        if (startup_mode == GPS_STARTUP_MODE_AUTO) {
            err = at_cmdw_set_ii(_at, "+CGPS", 1, 1, false);
        } else if (startup_mode == GPS_STARTUP_MODE_COLD) {
            err = at_cmdw_run(_at, "+CGPSDEL", false); // ensure that existed gps data is deleted
            err = any_error(err, at_cmdw_run(_at, "+CGPSCOLD", false));
        } else {
            err = at_cmdw_run(_at, "+CGPSHOT", false);
        }
    } else {
        // ensure switch to standalone mode automatically
        at_cmdw_set_i(_at, "+CGPSMSB", 1, false);
        // run gps
        err = at_cmdw_set_ii(_at, "+CGPS", 1, 2, false);
    }

    if (err) {
        return err;
    }

    // run gps timer
    _up_timer.reset();
    _up_timer.start();

    return _wait_gps_start();
}

/**
 * Fix sim5320 rollover bug:
 *
 *  See the following links for more details:
 *  - https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/284
 *  - https://www.cika.com/soporte/Information/GSMmodules/GPS-week-rollover_Simcom.pdf
 *
 */
static time_t fix_date_week_rollover(time_t t)
{
    // note: find better way to handle this issue
    // this time to week rollover period
    t += 7 * 1024 * 24 * 60 * 60;
    return t;
}

nsapi_error_t SIM5320LocationService::gps_read_coord(SIM5320LocationService::coord_t *coord, bool &ff_flag)
{
    char lat_str[16];
    char lat_dir_str[4];
    char log_str[16];
    char log_dir_str[4];
    char date_str[8];
    char utc_time_str[10];
    char alt_str[10];

    ATHandlerLocker locker(_at);

    _at.cmd_start("AT+CGPSINFO");
    _at.cmd_stop();
    _at.resp_start("+CGPSINFO:");
    // read response
    // example 1: 3113.343286,N,12121.234064,E,250311,072809.3,44.1,0.0,0
    // example 1: ,,,,,,,,
    _at.read_string(lat_str, 16);
    _at.read_string(lat_dir_str, 4);
    _at.read_string(log_str, 16);
    _at.read_string(log_dir_str, 4);
    _at.read_string(date_str, 8);
    _at.read_string(utc_time_str, 10);
    _at.read_string(alt_str, 10);
    // ignore speed and course
    _at.skip_param(2);
    _at.resp_start("AmpI/AmpQ:");
    _at.skip_param(2);
    _at.resp_stop();

    if (strlen(lat_str) == 0) {
        // no data
        ff_flag = false;
    } else {
        // has coordinates
        ff_flag = true;
        // parse coordinates
        float lat = strtof(lat_str, NULL);
        lat /= 100;
        lat = (int)lat + (lat - (int)lat) * (5.0f / 3.0f);
        if (lat_dir_str[0] == 'S') {
            lat = -lat;
        }
        float log = strtof(log_str, NULL);
        log /= 100;
        log = (int)log + (log - (int)log) * (5.0f / 3.0f);
        if (log_dir_str[0] == 'W') {
            log = -log;
        }
        float alt = strtof(alt_str, NULL);
        // parse date
        tm gps_tm;
        SimpleStringParser date_parser(date_str);
        date_parser.consume_int(&gps_tm.tm_mday, 2);
        date_parser.consume_int(&gps_tm.tm_mon, 2);
        date_parser.consume_int(&gps_tm.tm_year, 2);
        int sub_sec;
        SimpleStringParser time_parser(utc_time_str);
        time_parser.consume_int(&gps_tm.tm_hour, 2);
        time_parser.consume_int(&gps_tm.tm_min, 2);
        time_parser.consume_int(&gps_tm.tm_sec, 2);
        time_parser.consume_literal(".");
        time_parser.consume_int(&sub_sec);
        gps_tm.tm_year += 100; // tm_year since 1900
        gps_tm.tm_mon -= 1;
        // fill result
        coord->latitude = lat;
        coord->longitude = log;
        coord->altitude = alt;
        coord->time = mktime(&gps_tm);
        // fix rollover week issue
        coord->time = fix_date_week_rollover(coord->time);
    }
    _at.resp_stop();

    return _at.get_last_error();
}

int SIM5320LocationService::_gps_stop_internal(milliseconds_u32 &op_duration)
{
    ATHandlerLocker locker(_at);
    int err;
    milliseconds_u32 op_start = to_ms_u32(_up_timer.elapsed_time());

    if ((err = at_cmdw_set_i(_at, "+CGPS", 0, false))) {
        return err;
    }
    err = _wait_gps_stop();

    op_duration = to_ms_u32(_up_timer.elapsed_time()) - op_start;

    if (!err) {
        _up_timer.stop();
    }

    return err;
}

nsapi_error_t SIM5320LocationService::gps_stop()
{
    milliseconds_u32 op_duration;
    return _gps_stop_internal(op_duration);
}

nsapi_error_t SIM5320LocationService::_gps_locate_base_impl(SIM5320LocationService::coord_t *coord, bool &ff_flag, SIM5320LocationService::GPSMode mode, SIM5320LocationService::GPSStartupMode startup_mode, Callback<milliseconds_u32(int)> timeout_cb, milliseconds_u32 poll_period)
{
    int err = 0;
    milliseconds_u32 op_duration;
    milliseconds_u32 elapsed_time;
    milliseconds_u32 poll_elapsated;
    milliseconds_u32 op_start;

    ATHandlerLocker lock(_at);
    ff_flag = false;

    // run GPS, ignore current settings
    gps_start(mode, startup_mode);

    op_start = to_ms_u32(_up_timer.elapsed_time());
    // enable CGPSFTM URC codes
    _last_cgpsftm_urc_sats = 0;
    _last_cgpsftm_urc_timestamp = op_start;
    _at.at_cmd_discard("+CGPSFTM", "=", "%d", 1);

    // wait first fix
    while (true) {
        elapsed_time = to_ms_u32(_up_timer.elapsed_time()) - op_start;
        err = gps_read_coord(coord, ff_flag);
        if (err) {
            break;
        }
        if (ff_flag) {
            break;
        }
        if (elapsed_time > timeout_cb(_last_cgpsftm_urc_sats)) {
            break;
        }
        // ensure that we process URC code every second
        poll_elapsated = poll_period;
        while (poll_elapsated > 1s) {
            ThisThread::sleep_for(1s);
            _at.process_oob();
            poll_elapsated -= 1s;
        }
        ThisThread::sleep_for(poll_elapsated);
    }

    // disable CGPSFTM URC codes
    _at.at_cmd_discard("+CGPSFTM", "=", "%d", 0);
    // stop GPS
    _gps_stop_internal(op_duration);

    if (ff_flag) {
        // add stop operation compensation
        coord->time += op_duration.count() / 1000;
    } else {
        tr_debug("Cannot resolve coordinates with %i satellites in view", _last_cgpsftm_urc_sats);
    }

    return err;
}

// gps cold startup note:
// 1) open sky, good signal: < 35 sec
// 2) open sky, weak signal: ~ 100 sec
static constexpr milliseconds_u32 _GPS_POLL_PERIOD = 2s;
static constexpr milliseconds_u32 _GPS_RETRY_PERIOD = 32s;
// note: if at least one satellite is found, wait more
static milliseconds_u32 calc_ttf_timeout(int total_sat_in_view)
{
    if (total_sat_in_view <= 0) {
        return 32s;
    } else if (total_sat_in_view <= 2) {
        return 128s;
    } else {
        return 640s;
    }
}

nsapi_error_t SIM5320LocationService::gps_locate(SIM5320LocationService::coord_t *coord, bool &ff_flag, SIM5320LocationService::GPSMode mode)
{
    int err = 0;
    bool xtra_usage_flag;
    ATHandlerLocker at(_at);

    ff_flag = false;

    // First attempt. Try to use existed GPS settings.
    err = _gps_locate_base_impl(coord, ff_flag, mode, GPS_STARTUP_MODE_AUTO, callback(calc_ttf_timeout), _GPS_POLL_PERIOD);
    if (!err && ff_flag) {
        return 0;
    }
    _at.clear_error();
    tr_error("First attempts of gps coordinates resolving has failed. Clear data and try again ...");

    // Second attempt. Try to disable all GPS features and use cold start
    // disable xtra function
    err = gps_xtra_get(xtra_usage_flag);
    if (err || xtra_usage_flag) {
        xtra_usage_flag = false;
        gps_xtra_set(false);
    }
    // delay before second attempt
    ThisThread::sleep_for(_GPS_RETRY_PERIOD);

    // try to get gps coordinates again
    err = _gps_locate_base_impl(coord, ff_flag, GPS_MODE_STANDALONE, GPS_STARTUP_MODE_COLD, callback(calc_ttf_timeout), _GPS_POLL_PERIOD);
    if (xtra_usage_flag) {
        // restore xtra function
        gps_xtra_set(true);
    }
    if (!err && ff_flag) {
        return 0;
    }
    if (err) {
        tr_error("Second attempts of gps coordinates resolving has failed.");
    }

    // coordinates resolving attempts have failed
    ff_flag = false;
    return err;
}

nsapi_error_t SIM5320LocationService::gps_is_active(bool &flag)
{
    int state_flag;
    int mode_flag;
    int err;

    if ((err = at_cmdw_get_ii(_at, "+CGPS", state_flag, mode_flag))) {
        return err;
    }

    flag = state_flag;

    return err;
}

nsapi_error_t SIM5320LocationService::gps_clear_data()
{
    return at_cmdw_run(_at, "+CGPSDEL");
}

nsapi_error_t SIM5320LocationService::gps_set_accuracy(int value)
{
    return at_cmdw_set_i(_at, "+CGPSHOR", value);
}

nsapi_error_t SIM5320LocationService::gps_get_accuracy(int &value)
{
    return at_cmdw_get_i(_at, "+CGPSHOR", value);
}

nsapi_error_t SIM5320LocationService::gps_set_agps_server(const char *server, bool ssl)
{
    ATHandlerLocker locker(_at);

    // set agps server
    _at.at_cmd_discard("+CGPSURL", "=", "%s", server);

    // set ssl usage
    at_cmdw_set_b(_at, "+CGPSSSL", ssl, false);

    return _at.get_last_error();
}

nsapi_error_t SIM5320LocationService::gps_xtra_set(bool value)
{
    return at_cmdw_set_b(_at, "+CGPSXE", value);
}

nsapi_error_t SIM5320LocationService::gps_xtra_get(bool &value)
{
    return at_cmdw_get_b(_at, "+CGPSXE", value);
}

nsapi_error_t SIM5320LocationService::gps_xtra_download()
{
    ATHandlerLocker locker(_at);
    int err = 0;
    int res;
    int download_code;

    _at.cmd_start_stop("+CGPSXD", "=", "%d", 0);
    res = read_full_fuzzy_response(_at, true, true, "+CGPSXD:", "%i", &download_code);
    err = _at.get_last_error();
    if (!err && (res != 1 || download_code != 0)) {
        err = -1;
    }

    return err;
}

static bool str_starts_with(const char *str, const char *prefix)
{
    while (*prefix != '\0') {
        if (*str != *prefix || *str == '\0') {
            return false;
        }
    }
    return true;
}

static bool parse_ccinfo_field(const char *field, const char *fmt, int &value)
{
    char *num_end;

    while (*fmt != '\0') {
        if (*fmt == '%') {
            fmt++; // skip 'i'
            // parse number
            value = std::strtol(field, &num_end, 10);
            field = num_end - 1;
        } else if (*fmt != *field) {
            return false;
        }
        fmt++;
        field++;
    }

    return true;
}

nsapi_error_t SIM5320LocationService::cell_system_read_info(SIM5320LocationService::station_info_t *station_info, bool &has_data)
{
    ATHandlerLocker locker(_at);
    int err = 0;
    int value;
    const size_t buf_len = 20;
    char buf[buf_len];

    // reset station info
    has_data = false;

    _at.cmd_start_stop("+CCINFO", "");
    _at.resp_start("+CCINFO:");

    while (_at.info_resp()) {
        // check cell type
        err = _at.read_string(buf, buf_len);
        if (err < 0) {
            break;
        } else {
            err = 0;
        }
        if (strcmp(buf, "[SCELL]") != 0) {
            continue;
        }

        // reset station info
        station_info->mcc = -1;
        station_info->mnc = -1;
        station_info->lac = -1;
        station_info->cid = -1;
        station_info->signal_db = 0;
        station_info->network_type = 0;

        while (_at.read_string(buf, buf_len) >= 0) {
            // parse values
            if (parse_ccinfo_field(buf, "MCC:%i", value)) {
                station_info->mcc = value;
            } else if (parse_ccinfo_field(buf, "MNC:%i", value)) {
                station_info->mnc = value;
            } else if (parse_ccinfo_field(buf, "LAC:%i", value)) {
                station_info->lac = value;
            } else if (parse_ccinfo_field(buf, "ID:%i", value)) {
                station_info->cid = value;
            } else if (parse_ccinfo_field(buf, "RXLev:%idbm", value)) {
                station_info->signal_db = value;
            } else if (parse_ccinfo_field(buf, "RXLev:%i", value)) {
                station_info->signal_db = value;
            } else if (parse_ccinfo_field(buf, "UARFCN:%i", value)) {
                station_info->network_type = 1;
            }
        }

        if (station_info->mcc == -1 || station_info->mnc == -1 || station_info->lac == -1 || station_info->cid == -1) {
            // invalid string
            err = -1;
            break;
        }

        // skip other strings
        has_data = true;
        break;
    }

    if (has_data && station_info->mcc == 0 && station_info->mnc == 0) {
        // skip stub data
        has_data = false;
    }

    // reduce operation timeout as we can get buffer overflow
    _at.set_at_timeout(200);
    _at.resp_stop();
    _at.restore_at_timeout();
    if (_at.get_last_error() || _at.get_last_device_error().errCode) {
        // CCINFO can have a lot of output and we probably get buffer overflow
        _at.flush();
        _at.clear_error();
    }

    return err;
}
