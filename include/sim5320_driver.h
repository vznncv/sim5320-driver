#ifndef SIM5320_DRIVER_H
#define SIM5320_DRIVER_H

#include "mbed.h"
#include "sim5320_CellularDevice.h"
#include "sim5320_FTPClient.h"
#include "sim5320_LocationService.h"
#include "sim5320_TimeService.h"
#include "sim5320_utils.h"

namespace sim5320 {

/**
 * SIM5320 device driver.
 */
class SIM5320 : private NonCopyable<SIM5320> {
public:
    /**
     * Constructor.
     *
     * @param serial_ptr Serial interface
     * @param rts RTS pin of the Serial interface
     * @param cts CTS pin of the Serial interface
     * @param rst hardware reset pin
     */
    SIM5320(BufferedSerial *serial_ptr, PinName rts = NC, PinName cts = NC, PinName rst = NC);
    /**
     * Constructor.
     *
     * @param tx TX pin of the Serial interface
     * @param rx RX pin of the Serial interface
     * @param rts RTS pin of the Serial interface
     * @param cts CTS pin of the Serial interface
     * @param rst hardware reset pin
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
     * Helper shortcut to set credentials, that are needed for network usage.
     *
     * @param pin sim card pin or either NULL or empty string if sim doesn't require a pin
     * @param apn_name apn name or either NULL or empty string if apn settings shouldn't be changed
     * @param apn_user apn username
     * @param apn_password apn password
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t network_set_params(const char *pin = nullptr, const char *apn_name = nullptr, const char *apn_username = nullptr, const char *apn_password = nullptr);

    /**
     * Helper shortcut to start device and open network.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t network_up();

    /**
     * Helper shortcut to close network and stop device.
     *
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t network_down();

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

#if MBED_CONF_CELLULAR_USE_SMS
    /**
     * Get sms interface.
     *
     * @return
     */
    CellularSMS *get_sms();
#endif // MBED_CONF_CELLULAR_USE_SMS

    /**
     * Get cellular context interface.
     *
     * @return
     */
    CellularContext *get_context();

    /**
     * Get location service interface.
     *
     * @return
     */
    SIM5320LocationService *get_location_service();

    /**
     * Get ftp client.
     *
     * @return
     */
    SIM5320FTPClient *get_ftp_client();

    /**
     * Get time service.
     *
     * @return
     */
    SIM5320TimeService *get_time_service();

private:
    PinName _rts;
    PinName _cts;
    BufferedSerial *_serial_ptr;
    bool _cleanup_serial;

    PinName _rst;
    DigitalOut *_rst_out_ptr;

    SIM5320CellularDevice *_device;
    CellularInformation *_information;
    CellularNetwork *_network;
#if MBED_CONF_CELLULAR_USE_SMS
    CellularSMS *_sms;
#endif // MBED_CONF_CELLULAR_USE_SMS
    CellularContext *_context;
    SIM5320LocationService *_location_service;
    SIM5320FTPClient *_ftp_client;
    SIM5320TimeService *_time_service;

    int _startup_request_count;
    int _network_up_request_count;
    ATHandler *_at;

    static const int _STARTUP_TIMEOUT_MS = 32000;
    nsapi_error_t _reset_soft();
    nsapi_error_t _reset_hard();
    nsapi_error_t _skip_initialization_messages();
};
}

#endif // SIM5320_DRIVER_H
