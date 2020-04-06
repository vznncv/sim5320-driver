#ifndef SIM5320_CELLULARDEVICE_H
#define SIM5320_CELLULARDEVICE_H

#include "AT_CellularDevice.h"
#include "CellularContext.h"
#include "CellularInformation.h"
#include "CellularNetwork.h"
#include "CellularSMS.h"
#include "mbed.h"
#include "sim5320_FTPClient.h"
#include "sim5320_GPSDevice.h"
#include "sim5320_TimeService.h"

namespace sim5320 {

/**
 * SIM5320 cellular device implementation.
 */
class SIM5320CellularDevice : public AT_CellularDevice, private NonCopyable<SIM5320CellularDevice> {
public:
    SIM5320CellularDevice(FileHandle *fh);
    virtual ~SIM5320CellularDevice();

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
    virtual void set_timeout(int timeout);

    // AT_CellularDevice
    virtual nsapi_error_t init();

    /**
     * Get GPS device interface.
     *
     * @param fh
     * @return
     */
    virtual SIM5320GPSDevice *open_gps(FileHandle *fh);

    /**
     * Close GPS device interface.
     */
    virtual void close_gps();

    /**
     * Open FTP client interface.
     *
     * @param fh
     * @return
     */
    virtual SIM5320FTPClient *open_ftp_client(FileHandle *fh);

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
    virtual SIM5320TimeService *open_time_service(FileHandle *fh);

    /**
    * Close time service interface.
    */
    virtual void close_time_service();

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

protected:
    // AT_CellularDevice
    virtual AT_CellularInformation *open_information_impl(ATHandler &at);
    virtual AT_CellularNetwork *open_network_impl(ATHandler &at);
    virtual AT_CellularContext *create_context_impl(ATHandler &at, const char *apn, bool cp_req = false, bool nonip_req = false);
    virtual AT_CellularSMS *open_sms_impl(ATHandler &at);

    virtual SIM5320GPSDevice *open_gps_impl(ATHandler &at);
    virtual SIM5320FTPClient *open_ftp_client_impl(ATHandler &at);
    virtual SIM5320TimeService *open_time_service_impl(ATHandler &at);

protected:
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

        IterfaceType *open_interface(FileHandle *fh, SIM5320CellularDevice *device)
        {
            if (!_interface) {
                _interface = (device->*open_method_impl)(*device->get_at_handler(fh));
            }
            _ref_count++;
            return _interface;
        };
        void close_interface(SIM5320CellularDevice *device)
        {
            if (_interface) {
                _ref_count--;
                if (_ref_count <= 0) {
                    ATHandler *atHandler = &_interface->get_at_handler();
                    delete _interface;
                    _interface = NULL;
                    device->release_at_handler(atHandler);
                }
            }
        }
        void cleanup(SIM5320CellularDevice *device)
        {
            _ref_count = 1;
            close_interface(device);
        }
    };

    DeviceInterfaceManager<SIM5320GPSDevice, &SIM5320CellularDevice::open_gps_impl> _gps;
    DeviceInterfaceManager<SIM5320FTPClient, &SIM5320CellularDevice::open_ftp_client_impl> _ftp_client;
    DeviceInterfaceManager<SIM5320TimeService, &SIM5320CellularDevice::open_time_service_impl> _time_service;
};
}

#endif // SIM5320_CELLULARDEVICE_H
