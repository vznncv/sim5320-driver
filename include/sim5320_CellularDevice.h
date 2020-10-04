#ifndef SIM5320_CELLULARDEVICE_H
#define SIM5320_CELLULARDEVICE_H

#include "mbed.h"

#include "AT_CellularDevice.h"

#include "CellularContext.h"
#include "CellularInformation.h"
#include "CellularNetwork.h"
#include "CellularSMS.h"

#include "sim5320_CellularInformation.h"
#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularSMS.h"
#include "sim5320_FTPClient.h"
#include "sim5320_LocationService.h"
#include "sim5320_TimeService.h"

namespace sim5320 {

/**
 * SIM5320 cellular device implementation.
 */
class SIM5320CellularDevice : public AT_CellularDevice, private NonCopyable<SIM5320CellularDevice> {
public:
    SIM5320CellularDevice(FileHandle *fh);
    virtual ~SIM5320CellularDevice() override;

    /**
     * Perform basic module initialization to check if it works.
     *
     * @return 0 on success, otherwise non-zero value
     */
    virtual nsapi_error_t init_at_interface();

    /**
     * Set device power level.
     *
     * Power levels:
     * 0 - minimal functionality mode (only UART works)
     * 1 - full functionality mode
     *
     * @param func_level
     * @return 0 on success, otherwise non-zero value
     */
    virtual nsapi_error_t set_power_level(int func_level);

    /**
     * Get current device power level.
     *
     * @param func_level
     * @return 0 on success, otherwise non-zero value
     */
    virtual nsapi_error_t get_power_level(int &func_level);

    // CellularDevice
    virtual void set_timeout(int timeout) override;

    // AT_CellularDevice
    virtual nsapi_error_t init() override;

    static const size_t SUBSCRIBER_NUMBER_MAX_LEN = 16;

    /**
     * Get subscriber number.
     *
     * If subscriber isn't set, an empty string will be returned.
     *
     * @param number output buffer. It should have size at least SUBSCRIBER_NUMBER_MAX_LEN
     * @return 0 on success, otherwise non-zero value
     */
    virtual nsapi_error_t get_subscriber_number(char *number);

    /**
     * Set default subscriber number.
     *
     * @param number
     * @return
     */
    virtual nsapi_error_t set_subscriber_number(const char *number);

    //--------------------------------
    // Device interfaces
    //--------------------------------

    //
    // Base cellular device interfaces
    //

    virtual CellularInformation *open_information() override;
    virtual void close_information() override;

    virtual CellularNetwork *open_network() override;
    virtual void close_network() override;

#if MBED_CONF_CELLULAR_USE_SMS
    virtual CellularSMS *open_sms() override;
    virtual void close_sms() override;
#endif //MBED_CONF_CELLULAR_USE_SMS
    //
    // sim5320 specific interfaces
    //

    /**
     * Get location service interface.
     *
     * @param fh
     * @return
     */
    virtual SIM5320LocationService *open_location_service();

    /**
     * Close location service interface.
     */
    virtual void close_location_service();

    /**
     * Open FTP client interface.
     *
     * @param fh
     * @return
     */
    virtual SIM5320FTPClient *open_ftp_client();

    /**
     * Close FTP client interface.
     */
    virtual void close_ftp_client();

    /**
    * Open time service interface.
    *
    * @param fh
    * @return
    */
    virtual SIM5320TimeService *open_time_service();

    /**
    * Close time service interface.
    */
    virtual void close_time_service();

protected:
    // AT_CellularDevice
    virtual AT_CellularContext *create_context_impl(ATHandler &at, const char *apn, bool cp_req = false, bool nonip_req = false) override;

    // device specific implementation
    virtual SIM5320CellularInformation *open_information_base_impl(ATHandler &at);
    virtual SIM5320CellularNetwork *open_network_base_impl(ATHandler &at);
#if MBED_CONF_CELLULAR_USE_SMS
    virtual SIM5320CellularSMS *open_sms_base_impl(ATHandler &at);
#endif // MBED_CONF_CELLULAR_USE_SMS

    virtual SIM5320LocationService *open_location_service_base_impl(ATHandler &at);
    virtual SIM5320FTPClient *open_ftp_client_base_impl(ATHandler &at);
    virtual SIM5320TimeService *open_time_service_base_impl(ATHandler &at);

    /**
     * Helper class to close and open interfaces.
     */
    template <class IterfaceType, IterfaceType *(SIM5320CellularDevice::*open_method_impl)(ATHandler &at)>
    class DeviceInterfaceManager {
    private:
        int _ref_count;
        IterfaceType *_interface;

    public:
        DeviceInterfaceManager()
            : _ref_count(0)
            , _interface(nullptr){};
        ~DeviceInterfaceManager(){};

        IterfaceType *open_interface(SIM5320CellularDevice *device)
        {
            if (!_interface) {
                _interface = (device->*open_method_impl)(device->_at);
            }
            _ref_count++;
            return _interface;
        };
        void close_interface(SIM5320CellularDevice *device)
        {
            if (_interface) {
                _ref_count--;
                if (_ref_count <= 0) {
                    delete _interface;
                    _interface = NULL;
                }
            }
        }
        void cleanup(SIM5320CellularDevice *device)
        {
            _ref_count = 1;
            close_interface(device);
        }
    };

    DeviceInterfaceManager<SIM5320CellularInformation, &SIM5320CellularDevice::open_information_base_impl> _information_service;
    DeviceInterfaceManager<SIM5320CellularNetwork, &SIM5320CellularDevice::open_network_base_impl> _network_service;
#if MBED_CONF_CELLULAR_USE_SMS
    DeviceInterfaceManager<SIM5320CellularSMS, &SIM5320CellularDevice::open_sms_base_impl> _sms_service;
#endif // MBED_CONF_CELLULAR_USE_SMS

    DeviceInterfaceManager<SIM5320LocationService, &SIM5320CellularDevice::open_location_service_base_impl> _location_service;
    DeviceInterfaceManager<SIM5320FTPClient, &SIM5320CellularDevice::open_ftp_client_base_impl> _ftp_client;
    DeviceInterfaceManager<SIM5320TimeService, &SIM5320CellularDevice::open_time_service_base_impl> _time_service;
};
}

#endif // SIM5320_CELLULARDEVICE_H
