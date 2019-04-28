#ifndef SIM5320_DRIVER_H
#define SIM5320_DRIVER_H

#include "mbed.h"
#include "sim5320_CellularDevice.h"
#include "sim5320_FTPClient.h"
#include "sim5320_GPSDevice.h"

namespace sim5320 {

/**
 * SIM5320 device driver.
 */
class SIM5320 : private NonCopyable<SIM5320> {
public:
    /**
     * Constructor.
     *
     * @param serial_ptr Serial interface.
     * @param rts RTS pin of the Serial interface
     * @param cts CTS pin of the Serial interface
     * @param rst hardware reset pin (will be used when reset command fails)
     */
    SIM5320(UARTSerial *serial_ptr, PinName rts = NC, PinName cts = NC, PinName rst = NC);
    /**
     * Constructor.
     *
     * @param tx TX pin of the Serial interface
     * @param rx RX pin of the Serial interface
     * @param rts RTS pin of the Serial interface
     * @param cts CTS pin of the Serial interface
     * @param rst hardware reset pin (will be used when reset command fails)
     */
    SIM5320(PinName tx, PinName rx, PinName rts = NC, PinName cts = NC, PinName rst = NC);

private:
    /**
     * Common code of the constructors.
     */
    void _init_driver();

public:
    virtual ~SIM5320();

    /**
     * Start hardware UART flow control of the board and SIM5320.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t start_uart_hw_flow_ctrl();

    /**
     * Stop hardware UART flow control of the board and SIM5320.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t stop_uart_hw_flow_ctrl();

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
    nsapi_error_t init();

    /**
     * Set all setting to factory values.
     *
     * After that module should be reset manually.
     *
     * @warning
     * Don't use this method in the production systems
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t set_factory_settings();

    /**
     * Send requests to start module.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t request_to_start();

    /**
     * Send requests to stop module.
     *
     * This action stop module if any code that send request to start module,
     * also send requests to stop module.
     *
     * When module is stopped, its current consumption will be reduced to the minimal level,
     * and any functionality won't be available (except UART port).
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t request_to_stop();

    /**
     * Check and process URC messages.
     *
     * @return
     */
    nsapi_error_t process_urc();

    enum ResetMode {
        RESET_MODE_DEFAULT = 0,
        RESET_MODE_SOFT = 1,
        RESET_MODE_HARD = 2
    };

    /**
     * Reset device.
     *
     * @param reset_mode
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t reset(ResetMode reset_mode = RESET_MODE_DEFAULT);

    //    /**
    //     * Wait till device is registered in the network
    //     *
    //     * @param timeout_ms maximal wait timeout
    //     * @return 0 on success, non-zero on failure
    //     */
    //    nsapi_error_t wait_network_registration(int timeout_ms = 30000);

    /**
     * Check if module is run.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t is_active(bool &active);

    /**
     * Get cellular device interface.
     *
     * @return
     */
    CellularDevice *get_device();

    /**
     * Get device information interface.
     *
     * @return
     */
    CellularInformation *get_information();

    /**
     * Get network interface.
     *
     * @return
     */
    CellularNetwork *get_network();

    /**
     * Get sms interface.
     *
     * @return
     */
    CellularSMS *get_sms();

    /**
     * Get cellular context interface.
     *
     * @return
     */
    CellularContext *get_context();

    /**
     * Get gps interface.
     *
     * @return
     */
    SIM5320GPSDevice *get_gps();

    /**
     * Get ftp client.
     *
     * @return
     */
    SIM5320FTPClient *get_ftp_client();

private:
    PinName _rts;
    PinName _cts;
    UARTSerial *_serial_ptr;
    bool _cleanup_uart;

    PinName _rst;
    DigitalOut *_rst_out_ptr;

    SIM5320CellularDevice *_device;
    CellularInformation *_information;
    CellularNetwork *_network;
    CellularSMS *_sms;
    CellularContext *_context;
    SIM5320GPSDevice *_gps;
    SIM5320FTPClient *_ftp_client;

    int _startup_request_count;
    ATHandler *_at;

    static const int _STARTUP_TIMEOUT_MS = 32000;
    nsapi_error_t _reset_soft();
    nsapi_error_t _reset_hard();
    nsapi_error_t _skip_initialization_messages();
};
}

#endif // SIM5320_DRIVER_H
