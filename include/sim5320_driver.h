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

class SIM5320 : private NonCopyable<SIM5320> {
public:
    /**
     * Constructor.
     *
     * @param serial_ptr Serial interface.
     */
    SIM5320(UARTSerial* _serial_ptr, PinName rts = NC, PinName cts = NC);
    /**
     * Constructor
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
    * Check AT command interface of SIM5320
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

    enum State {
        CLEANUP_SERIAL = 0x01,
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
