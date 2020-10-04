#ifndef SIM5320_LOCATIONSERVICE_H
#define SIM5320_LOCATIONSERVICE_H

#include "mbed.h"
#include "mbed_chrono.h"

#include "ATHandler.h"

namespace sim5320 {

#ifdef DEVICE_LPTICKER
using TargetTimer = LowPowerTimer;
#else
using TargetTimer = Timer;
#endif

/**
 * Location API of the SIM5320.
 *
 * It includes API for GPS and cellular base station information extraction
 */
class SIM5320LocationService : private NonCopyable<SIM5320LocationService> {

protected:
    // helper timer to measure time since gps startup
    TargetTimer _up_timer;

    ATHandler &_at;

private:
    /**
     * CGPSFTM URC code processing (see AT+CGPSFTM).
     *
     * It's used to check if any satellites are detected.
     */
    void _cgpsftm_urc();

    mbed::chrono::milliseconds_u32 _last_cgpsftm_urc_timestamp;
    int _last_cgpsftm_urc_sats;

public:
    SIM5320LocationService(ATHandler &at);
    virtual ~SIM5320LocationService();

    /**
     * Perform initial GPS configuration and set some default settings.
     */
    nsapi_error_t init();

    /**
     * Coordinates.
     */
    struct coord_t {
        /** current longitude */
        float longitude;
        /** current latitude */
        float latitude;
        /** current altitude */
        float altitude;
        /** current time */
        time_t time;
    };

    /**
     * Base station information.
     */
    struct station_info_t {
        /** mobile country code */
        int mcc;
        /** mobile network code */
        int mnc;
        /** localization area code */
        int lac;
        /** cell identifier */
        int cid;
        /** signal power */
        int signal_db;
        /**
         * network type:
         * 0 - GSM (2G)
         * 1 - WCDMA (3G)
         */
        int network_type;
    };

    /**
     * The GPS mode.
     */
    enum GPSMode {
        /** GPS standalone mode.
         *
         * It doesn't require network
         */
        GPS_MODE_STANDALONE = 0,
        /**
         * UE based mode.
         *
         * It has better performance, but requires network.
         * If it fails, GPS will be switched to standalone mode automatically.
         *
         * To use it, you should set AGPS server with ::gps_set_agps_server method.
         */
        GPS_MODE_UE_BASED = 1
    };

    /**
     * Standalone mode startup options.
     */
    enum GPSStartupMode {
        /** Select mode automatically. */
        GPS_STARTUP_MODE_AUTO = 0,
        /** Cold startup mode */
        GPS_STARTUP_MODE_COLD = 1,
        /** Hot startup mode */
        GPS_STARTUP_MODE_HOT = 2
    };

private:
    //****
    // GPS receivers timeout constants
    //****
    // receiver requires some time for starting/stopping
    static constexpr mbed::chrono::milliseconds_u32 _GPS_START_TIMEOUT = 8s;
    static constexpr mbed::chrono::milliseconds_u32 _GPS_STOP_TIMEOUT = 32s;
    static constexpr mbed::chrono::milliseconds_u32 _GPS_SS_CHECK_PERIOD = 1s;

    //
    // After AT+CGPS command GPS may require 1-3 seconds to be started/stopped.
    // So we need some extra logic to process it.
    //
    nsapi_error_t _wait_gps_start_stop(bool state, mbed::chrono::milliseconds_u32 timeout, mbed::chrono::milliseconds_u32 check_period);
    nsapi_error_t _wait_gps_start(mbed::chrono::milliseconds_u32 timeout = _GPS_START_TIMEOUT, mbed::chrono::milliseconds_u32 check_period = _GPS_SS_CHECK_PERIOD)
    {
        return _wait_gps_start_stop(true, timeout, check_period);
    }
    nsapi_error_t _wait_gps_stop(mbed::chrono::milliseconds_u32 timeout = _GPS_STOP_TIMEOUT, mbed::chrono::milliseconds_u32 check_period = _GPS_SS_CHECK_PERIOD)
    {
        return _wait_gps_start_stop(false, timeout, check_period);
    }

public:
    /**
     * Start GPS.
     *
     * @param mode GPS mode
     * @param startup_mode GPS startup mode
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_start(GPSMode mode = GPS_MODE_STANDALONE, GPSStartupMode startup_mode = GPS_STARTUP_MODE_AUTO);

    /**
     * Read current GPS coordinates.
     *
     * The coordinates should be availables after 2-30 seconds after GPS startup.
     * If coordinates aren't availible after 2-3 minutes of the GPS startup,
     * it's recommended to stop GPS and try to get coordinates later.
     *
     * @param coord gps coordinates
     * @param ff_flag first fix flag. If output value is @c true, then GPS coordinates are available,
     *                otherwise they aren't captured, and @p coord isn't filled.
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_read_coord(coord_t *coord, bool &ff_flag);

private:
    int _gps_stop_internal(mbed::chrono::milliseconds_u32 &op_duration);

public:
    /**
     * Stop GPS.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_stop();

private:
    /**
     * Run GPS with current settings, try to get coordinates and stop.
     */
    nsapi_error_t _gps_locate_base_impl(coord_t *coord, bool &ff_flag, GPSMode mode, GPSStartupMode startup_mode, Callback<mbed::chrono::milliseconds_u32(int)> timeout_cb, mbed::chrono::milliseconds_u32 poll_period);

public:
    /**
     * Locate current coordinates.
     *
     * It's convenient wrapper around ::gps_start, ::gps_read_coord and ::gps_stop methods methods,
     * but it's execution can take up to 5 minutes.
     * It's recommended to use this method, unless you want to use event queue for better resource utilization.
     *
     * If the device doesn't resolve current coordinates, the @p ff_flag will be @c false, otherwise @c true.
     *
     * @param coord gps coordinates
     * @param ff_flag success flag
     * @param mode GPS mode
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_locate(coord_t *coord, bool &ff_flag, GPSMode mode = GPS_MODE_STANDALONE);

    /**
     * Check if GPS is run.
     *
     * @param flag @c true if GPS is run, otherwise @c false
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_is_active(bool &flag);

    /**
     * Clear stored almanac and ephemeris data.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_clear_data();

    /**
     * Set desired GPS accuracy.
     *
     * @param value accuracy threshold in meters
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_set_accuracy(int value);

    /**
     * Get desired GPS accuracy.
     *
     * @param value value accuracy threshold in meters
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_get_accuracy(int &value);

    /**
     * Set AGPS server for UE based mode.
     *
     * @param server server URL "<domain>[:port]"
     * @param ssl if @c true, then use SSL, otherwise use without SSL
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_set_agps_server(const char *server, bool ssl);

    /**
     * Enable/disable gpsOneXTRA technology.
     *
     * It allows to use assistance data file that reduced TTFF (time to first fix).
     *
     * To use it, you should:
     * - download assistant file with ::gps_xtra_download. The file is valid up to 7 days.
     * - set correct time to modem
     * - enable xtra technology
     * - start gps
     *
     * @param value boolean flag
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_xtra_set(bool value);
    /**
     * Check if gpsOneXTRA is enabled/disabled.
     *
     * @param value boolean flag
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_xtra_get(bool &value);

    /**
     * Download/update gpsOneXTRA file.
     *
     * To download it, network should be configured.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t gps_xtra_download();

    /**
     * Get information about current cellular station.
     *
     * It can be used to get coordinate estimation if GPS data isn't available.
     *
     * If data isn't available, then @param has_data is set to @code false , otherwise @code true .
     *
     * @param station_info cellular station information
     * @param has_data data aviability flag
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t cell_system_read_info(station_info_t *station_info, bool &has_data);
};
}

#endif // SIM5320_LOCATIONSERVICE_H
