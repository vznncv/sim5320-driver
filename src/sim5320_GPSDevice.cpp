#include "sim5320_GPSDevice.h"
#include "sim5320_utils.h"
using namespace sim5320;

#define GPS_START_STOP_CHECK_DELAY 2000
#define GPS_START_STOP_CHECK_NUM 10

SIM5320GPSDevice::SIM5320GPSDevice(ATHandler &at)
    : AT_CellularBase(at)
{
}

SIM5320GPSDevice::~SIM5320GPSDevice()
{
}

nsapi_error_t SIM5320GPSDevice::start(SIM5320GPSDevice::Mode gps_mode)
{
    ATHandlerLocker locker(_at, GPS_START_STOP_CHECK_NUM * GPS_START_STOP_CHECK_DELAY);

    // default settings
    // set AGPS url
    _at.cmd_start("AT+CGPSURL=");
    _at.write_string(get_assist_server_url());
    _at.cmd_stop_read_resp();
    // set AGPS ssl usage
    _at.cmd_start("AT+CGPSSSL=");
    _at.write_int(use_assist_server_ssl() ? 1 : 0);
    _at.cmd_stop_read_resp();
    // disable automatic (AT+CGPSAUTO) GPS start
    _at.cmd_start("AT+CGPSAUTO=0");
    _at.cmd_stop_read_resp();
    // set position mode (AT+CGPSPMD) to 127
    _at.cmd_start("AT+CGPSPMD=127");
    _at.cmd_stop_read_resp();
    // ensure switch to standalone mode automatically
    _at.cmd_start("AT+CGPSMSB=1");
    _at.cmd_stop_read_resp();

    int mode_code = gps_mode + 1;
    _at.cmd_start("AT+CGPS=");
    _at.write_int(1);
    _at.write_int(mode_code);
    _at.cmd_stop_read_resp();

    // wait startup
    bool active = false;
    int i = 0;
    int err;
    while (true) {
        err = is_active(active);
        if (err) {
            return err;
        }
        if (active) {
            break;
        }
        i++;
        if (i >= GPS_START_STOP_CHECK_NUM) {
            return MBED_ERROR_CODE_TIME_OUT;
        }
        ThisThread::sleep_for(GPS_START_STOP_CHECK_DELAY);
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320GPSDevice::stop()
{
    ATHandlerLocker locker(_at, GPS_START_STOP_CHECK_NUM * GPS_START_STOP_CHECK_DELAY);

    _at.cmd_start("AT+CGPS=");
    _at.write_int(0);
    _at.cmd_stop_read_resp();

    // wait shutdown
    bool active = true;
    int i = 0;
    int err;
    while (true) {
        err = is_active(active);
        if (err) {
            return err;
        }
        if (!active) {
            break;
        }
        i++;
        if (i >= GPS_START_STOP_CHECK_NUM) {
            return MBED_ERROR_CODE_TIME_OUT;
        }
        ThisThread::sleep_for(GPS_START_STOP_CHECK_DELAY);
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320GPSDevice::is_active(bool &active)
{
    ATHandlerLocker locker(_at);

    int on_flag;
    _at.cmd_start("AT+CGPS?");
    _at.cmd_stop();
    _at.resp_start("+CGPS:");
    on_flag = _at.read_int();
    _at.read_int(); // read mode
    _at.resp_stop();

    active = on_flag;
    return _at.get_last_error();
}

const static SIM5320GPSDevice::Mode GPS_MODE_MAP[] = {
    SIM5320GPSDevice::STANDALONE_MODE,
    SIM5320GPSDevice::STANDALONE_MODE,
    SIM5320GPSDevice::UE_BASED_MODE,
};

nsapi_error_t SIM5320GPSDevice::get_mode(SIM5320GPSDevice::Mode &mode)
{
    ATHandlerLocker locker(_at);

    int mode_code;
    _at.cmd_start("AT+CGPS?");
    _at.cmd_stop();
    _at.resp_start("+CGPS:");
    mode_code = _at.read_int(); // read state
    _at.read_int();
    _at.resp_stop();

    mode_code = mode_code < 0 ? 0 : mode_code;
    mode_code = mode_code > 2 ? 2 : mode_code;
    mode = GPS_MODE_MAP[mode_code];

    return _at.get_last_error();
}

nsapi_error_t SIM5320GPSDevice::set_desired_accuracy(int value)
{
    ATHandlerLocker locker(_at);

    _at.cmd_start("AT+CGPSHOR=");
    _at.write_int(value);
    _at.cmd_stop_read_resp();

    return _at.get_last_error();
}

nsapi_error_t SIM5320GPSDevice::get_desired_accuracy(int &value)
{
    ATHandlerLocker locker(_at);

    _at.cmd_start("AT+CGPSHOR?");
    _at.cmd_stop();
    _at.resp_start("+CGPSHOR:");
    value = _at.read_int();
    _at.resp_stop();

    return _at.get_last_error();
}

nsapi_error_t SIM5320GPSDevice::get_coord(bool &has_coordinates, SIM5320GPSDevice::gps_coord_t &coord)
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
        has_coordinates = false;
    } else {
        // has coordinates
        has_coordinates = true;
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
        coord.latitude = lat;
        coord.longitude = log;
        coord.altitude = alt;
        coord.time = mktime(&gps_tm);
    }
    _at.resp_stop();

    return _at.get_last_error();
}

const char *SIM5320GPSDevice::get_assist_server_url()
{
    return "supl.google.com:7276";
}

bool SIM5320GPSDevice::use_assist_server_ssl()
{
    return false;
}
