#ifndef SIM5320_DRIVER_H
#define SIM5320_DRIVER_H

#include "mbed.h"

namespace sim5320 {

struct gps_coord_t {
    // current longitude
    float longitude;
    // current latitude
    float latitude;
    // current altitude
    float altitude;
    // current time
    time_t time;
};

/**
 * SIM5320 device driver.
 */
class SIM5320 : private NonCopyable<SIM5320> {
public:
    /**
     * Constructor.
     *
     * @param serial_ptr Serial interface.
     */
    SIM5320(UARTSerial* _serial_ptr, PinName rts = NC, PinName cts = NC);
    /**
     * Constructor.
     *
     * @param tx TX pin of the Serial interface
     * @param rx RX pin of the Serial interface
     * @param rts RTS pin of the Serial interface
     * @param cts CTS pin of the Serial interface
     */
    SIM5320(PinName tx, PinName rx, PinName rts = NC, PinName cts = NC);

    virtual ~SIM5320();

    /**
     * Enabled/disable debug mode.
     *
     * @param on
     */
    void debug_on(bool on);

    /**
    * Check AT command interface of SIM5320.
    *
    * @return true if it's ready for communication
    */
    bool at_available(void);

    /**
     * Start hardware UART flow control of the board and SIM5320.
     *
     * @return true if it's started
     */
    bool start_uart_hw_flow_ctrl();

    /**
     * Stop hardware UART flow control of the board and SIM5320.
     *
     * @return true if it's stopped
     */
    bool stop_uart_hw_flow_ctrl();

    /**
     * Initialize device.
     *
     * It should be invoke before device usage.
     *
     * The method checks that SIM5320 is ready for communication, set some default settings
     * and switch it into low power mode, if it's not in the this mode.
     *
     * @return error code if error occurs
     */
    int init();

    /**
     * Reset module.
     *
     * @note
     * The function will wait till module startup. It can take several seconds.
     *
     * @return true if reset command has been executed successfully
     */
    bool reset();

    /**
     * Get string with international mobile station equipment identity.
     *
     * @param imei
     */
    void get_imei(char imei[18]);

    /**
     * Start module.
     *
     * @return true, if module has been run.
     */
    bool start();

    /**
     * Stop module.
     *
     * This action switches module into low power mode.
     * In this mode current consumption will be reduced to the minimal level,
     * but any functionality won't be available (except UART port).
     *
     * @return true, if module has been stopped.
     */
    bool stop();

    /**
     * Check if module is run.
     *
     * @return
     */
    bool is_active();

    //----------------------------------------------------------------
    // Common network function
    //----------------------------------------------------------------

    /**
     * Wait till device is registered in the network.
     *
     * If device is registered in the network, you can send SMS and send/receive data.
     *
     * @param timeout
     * @return true if it's succeed, otherwise false
     */
    bool network_wait_registration(int timeout = 16000);

    /**
     * Check if device is registered in the network.
     *
     * @return
     */
    bool network_is_registered();

    /**
     * Get network signal strength in dBm.
     *
     * If network isn't available, NaN is returned.
     *
     * @return
     */
    float network_get_signal_strength();

    enum NetworkType {
        NETWORK_TYPE_NO_SERVICE = 0, // no network
        NETWORK_TYPE_GSM = 1, // 2G network
        NETWORK_TYPE_GPRS = 2, // 2G network
        NETWORK_TYPE_EGPRS = 3, // EDGE, 2.5G network
        NETWORK_TYPE_WCDMA = 4, // 3G network
        NETWORK_TYPE_HSPA = 5, // 3.5G network
    };

    /**
     * Get current network type.
     *
     * @return
     */
    NetworkType network_get_type();

    /**
     * Get operator name;
     *
     * @param name output string (it should contain at least place for 64 symbols)
     */
    void network_get_operator_name(char name[64]);

    enum NetworkDPDType {
        NETWORK_PDP_TYPE_IP = 0,
        NETWORK_PDP_TYPE_PPP = 1,
        NETWORK_PDP_TYPE_IPV6 = 2
    };

    /**
     * Configure network context.
     *
     * This settings should be set if you want to use IP protocols.
     *
     * @param apn apn point name
     * @param user user name
     * @param password user password
     * @param pdp_type pdp type
     * @return true if network has been configure successfully.
     */
    bool network_configure_context(const char* apn, const char* user, const char* password, NetworkDPDType pdp_type = NETWORK_PDP_TYPE_IP);

    /**
     * Get current IP address.
     *
     * @param ip_address the string will be empty if there network isn't configured
     */
    void network_get_device_ip_address(char ip_address[46]);

    //----------------------------------------------------------------
    // SMS functions
    //----------------------------------------------------------------

    /**
     * Send SMS.
     *
     * @param number destination number
     * @param text message text
     * @return true if message is send successfully
     */
    bool sms_send_message(const char* number, const char* text);

    //----------------------------------------------------------------
    // FTP functions
    //----------------------------------------------------------------

private:
    int _ftp_read_ret_code(const char* command_name);

public:
    enum FTPProtocol {
        FTP_PROTOCOL_FTP = 0,
        FTP_PROTOCOL_FTPS_SSL = 1,
        FTP_PROTOCOL_FTPS_TLS = 2,
        FTP_RPOTOCOL_FTPS = 3
    };

    /**
     * Connect to ftp server.
     *
     * @param host ftp server host
     * @param port ftp port
     * @param protocol ftp protocol (ftp or ftps)
     * @param username username. If it isn't set then "anonymous"
     * @param password user password
     * @return true if operation succeeds
     */
    bool ftp_connect(const char* host, int port = 21, FTPProtocol protocol = FTP_PROTOCOL_FTP, const char* username = "anonymous", const char* password = "");

    /**
     * Get current working directory.
     *
     * @param work_dir
     * @return true if operation succeeds
     */
    bool ftp_get_cwd(char work_dir[256]);

    /**
     * Set current working directory.
     *
     * @param work_dir
     * @return if operation succeeds
     */
    bool ftp_set_cwd(const char* work_dir);

    /**
     * List files in the specified directory.
     *
     * For each filename the @p name_callback will be invoked with `filename`.
     * To get full filepath you should concatenate @p path and `filename`.
     *
     * @param path directory path
     * @param name_callback callback
     * @return true if operation succeeds
     */
    bool ftp_list_dir(const char* path, Callback<void(const char*)> name_callback);

    /**
     * Check if file exists on a ftp server.
     *
     * @param path file path
     * @param result true if file exists, otherwise false
     * @return true if operation succeeds
     */
    bool ftp_exists(const char* path, bool& result);

    /**
     * Create a directory on a ftp server.
     *
     * @param path directory path
     * @return true if operation succeeds
     */
    bool ftp_mkdir(const char* path);

    /**
     * Remove a directory on a ftp server.
     *
     * @param path directory path
     * @return true if operation succeeds
     */
    bool ftp_rmdir(const char* path);

    /**
     * Remove a file on a ftp server.
     *
     * @param path file path
     * @return true if operation succeeds
     */
    bool ftp_rmfile(const char* path);

    /**
     * Put file on an ftp server.
     *
     * The data source is callback @p data_reader. It accept buffer `data` and maximum data length `size`
     * that it can accept. The callback should return actual amount of data that it put into `data`.
     * If returned value is `0`, then transmission will be finished.
     *
     * @param path ftp file path
     * @param data_reader callback to provide data
     * @return true if operation succeeds
     */
    bool ftp_put(const char* path, Callback<ssize_t(uint8_t* data, ssize_t size)> data_reader);

    /**
     * Get file from ftp server.
     *
     * The read data will be processed by @p data_writer callback. It accepts buffer `data`, its length `size`,
     * and returns amount of the data that has been processed.
     *
     * @param path ftp file path
     * @param data_writer callback
     * @return true if operation succeeds
     */
    bool ftp_get(const char* path, Callback<ssize_t(uint8_t* data, ssize_t size)> data_writer);

    /**
     * @brief ftp_close
     * @return
     */
    bool ftp_close();

    //----------------------------------------------------------------
    // GPS function
    //----------------------------------------------------------------

    /**
     * Perform base GPS initialization.
     *
     * This method sets some default settings and switches off GPS if it's enabled.
     */
    int gps_init();

    enum GPSMode {
        GPS_STANDALONE_MODE = 0,
        GPS_UE_BASED_MODE = 1
    };

    /**
     * Run GPS.
     *
     * @return true if GPS is run
     */
    bool gps_start(GPSMode gps_mode = GPS_UE_BASED_MODE);

    /**
     * Stop GPS.
     *
     * @return true if GPS is stopped
     */
    bool gps_stop();

    /**
     * Check if GPS is run.
     *
     * @return
     */
    bool gps_is_active();

    /**
     * Get current GPS mode.
     *
     * @return
     */
    GPSMode get_gps_mode();

    /**
     * Get current coordinate.
     *
     * @param coord structure that will be filled with coordinates
     * @return true if coordinates are available, otherwise false
     */
    bool gps_get_coord(gps_coord_t* coord);

private:
    UARTSerial* _serial_ptr;
    ATCmdParser _parser;
    int _last_error_code;
    int _last_ftp_error_code;

    void _configure_oob();
    void _check_command_retcode();

    enum State {
        STATE_CLEANUP_SERIAL = 0x01,
        STATE_DEBUG_ON = 0x02
    };
    // helper variables to hold some driver parameters
    uint8_t _state;

    PinName _rts;
    PinName _cts;

    // default parser settings
    const static int SERIAL_BAUDRATE;
    const static int AT_BUFFER_SIZE;
    const static int AT_TIMEOUT;
    const static char* AT_DELIMITER;

    // default GPS settings
    const static char* GPS_ASSIST_SERVER_URL;
    const static bool GPS_ASSIST_SERVER_SSL;

    // to simplify internal logic only one PDP context will be used
    const static int PDP_CONTEXT_NO = 1;

    PlatformMutex _mutex;

    // helper class to lock mutex using RAII approach
    class DriverLock : private NonCopyable<DriverLock> {
    public:
        DriverLock(SIM5320* _instance_ptr);

        ~DriverLock();

    private:
        SIM5320* _instance_ptr;
    };
};
}

#endif // SIM5320_DRIVER_H
