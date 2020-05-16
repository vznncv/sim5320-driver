#include "sim5320_LocationService.h"
#include "sim5320_utils.h"

#include "mbed_trace.h"
#define TRACE_GROUP "smlc"

#ifdef DEVICE_LPTICKER
#define TargetTimer LowPowerTimer
#else
#define TargetTimer Timer
#endif

using namespace sim5320;

SIM5320LocationService::SIM5320LocationService(ATHandler &at, AT_CellularDevice &device)
    : _at(at)
    , _device(device)
{
}

SIM5320LocationService::~SIM5320LocationService()
{
}

nsapi_error_t SIM5320LocationService::init()
{
    ATHandlerLocker locker(_at);

    // disable automatic (AT+CGPSAUTO) GPS start
    at_cmdw_set_i(_at, "+CGPSAUTO", 0, false);
    // set position mode (AT+CGPSPMD) to 127
    at_cmdw_set_i(_at, "+CGPSPMD", 127, false);

    return _at.get_last_error();
}

nsapi_error_t SIM5320LocationService::_wait_gps_start_stop(bool state, int timeout_ms, int check_period_ms)
{
    TargetTimer tm;
    int elapsed_time;
    ATHandlerLocker locker(_at, timeout_ms + 1000);
    tm.start();

    bool active = !state;
    int err;

    while (true) {
        elapsed_time = tm.read_ms();
        err = gps_is_active(active);
        if (err) {
            return err;
        }
        if (state == active) {
            break;
        }
        if (elapsed_time > timeout_ms) {
            break;
        }
        ThisThread::sleep_for(check_period_ms);
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
        sscanf(date_str, "%2d%2d%2d", &gps_tm.tm_mday, &gps_tm.tm_mon, &gps_tm.tm_year);
        float tm_sec;
        sscanf(utc_time_str, "%2d%2d%f", &gps_tm.tm_hour, &gps_tm.tm_min, &tm_sec);
        gps_tm.tm_sec = (int)tm_sec;
        gps_tm.tm_year += 100;
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

int SIM5320LocationService::_gps_stop_internal(int &op_duration_ms)
{
    ATHandlerLocker locker(_at);
    TargetTimer t;
    int err;

    t.start();

    if ((err = at_cmdw_set_i(_at, "+CGPS", 0, false))) {
        return err;
    }
    err = _wait_gps_stop();

    t.stop();
    op_duration_ms = t.read_ms();
    return err;
}

nsapi_error_t SIM5320LocationService::gps_stop()
{
    int op_duration_ms;
    return _gps_stop_internal(op_duration_ms);
}

nsapi_error_t SIM5320LocationService::_gps_locate_base_impl(SIM5320LocationService::coord_t *coord, bool &ff_flag, SIM5320LocationService::GPSMode mode, SIM5320LocationService::GPSStartupMode startup_mode, int timeout_ms, int check_period_ms)
{
    int err = 0;
    int op_duration_ms;
    TargetTimer tm;
    int elapsed_time;

    ATHandlerLocker lock(_at);
    ff_flag = false;

    // run GPS, ignore current settings
    gps_start(mode, startup_mode);
    // wait first fix
    tm.start();
    while (true) {
        elapsed_time = tm.read_ms();
        err = gps_read_coord(coord, ff_flag);
        if (err) {
            break;
        }
        if (ff_flag) {
            break;
        }
        if (elapsed_time > timeout_ms) {
            break;
        }
        ThisThread::sleep_for(check_period_ms);
    }
    tm.stop();
    // stop GPS
    _gps_stop_internal(op_duration_ms);

    if (ff_flag) {
        // add stop operation compensation
        coord->time += op_duration_ms / 1000;
    }

    return err;
}

nsapi_error_t SIM5320LocationService::gps_locate(SIM5320LocationService::coord_t *coord, bool &ff_flag, SIM5320LocationService::GPSMode mode)
{
    int err = 0;
    bool xtra_usage_flag;
    bool res_flag = false;
    int op_duration_ms;
    ATHandlerLocker at(_at);

    ff_flag = false;

    // First attempt. Try to use existed GPS settings.
    err = _gps_locate_base_impl(coord, ff_flag, mode, GPS_STARTUP_MODE_AUTO, _GPS_COLD_TTF_TIMEOUT_MS, _GPS_CHECK_PERIOD_MS);
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
    ThisThread::sleep_for(_GPS_RETRY_PERIOD_MS);

    // try to get gps coordinates again
    err = _gps_locate_base_impl(coord, ff_flag, GPS_MODE_STANDALONE, GPS_STARTUP_MODE_COLD, _GPS_COLD_TTF_TIMEOUT_MS, _GPS_CHECK_PERIOD_MS);
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
