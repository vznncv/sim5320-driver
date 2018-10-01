#ifndef SIM5320_CELLULARDEVICE_H
#define SIM5320_CELLULARDEVICE_H
#include "AT_CellularDevice.h"
#include "AT_CellularInformation.h"
#include "AT_CellularNetwork.h"
#include "AT_CellularPower.h"
#include "AT_CellularSIM.h"
#include "AT_CellularSMS.h"
#include "AT_CellularStack.h"

#include "mbed.h"
#include "sim5320_FTPClient.h"
#include "sim5320_GPSDevice.h"

namespace sim5320 {

class SIM5320;

class SIM5320CellularDevice : public AT_CellularDevice, private NonCopyable<SIM5320CellularDevice> {
public:
    SIM5320CellularDevice(events::EventQueue& queue);
    virtual ~SIM5320CellularDevice();

    // expose ATHandler API for flow control configuration
    // Note: the EasyCellular isn't used as it has hard coded configuration
    friend SIM5320;

    virtual SIM5320GPSDevice* open_gps(FileHandle* fh);
    virtual void close_gps();
    virtual SIM5320FTPClient* open_ftp_client(FileHandle* fh);
    virtual void close_ftp_client();

protected:
    virtual AT_CellularPower* open_power_impl(ATHandler& at);
    virtual AT_CellularInformation* open_information_impl(ATHandler& at);
    virtual AT_CellularNetwork* open_network_impl(ATHandler& at);

    virtual SIM5320GPSDevice* open_gps_impl(ATHandler& at);
    virtual SIM5320FTPClient* open_ftp_client_impl(ATHandler& at);

    SIM5320GPSDevice* _gps;
    int _gps_ref_count;
    SIM5320FTPClient* _ftp_client;
    int _ftp_client_ref_count;
};
}

#endif // SIM5320_CELLULARDEVICE_H
