#ifndef SIM5320_CELLULARCONTEXT_H
#define SIM5320_CELLULARCONTEXT_H
#include "AT_CellularContext.h"
#include "mbed.h"
#include "sim5320_CellularDevice.h"

namespace sim5320 {

/**
 * SIM5320 cellular context implementation.
 */
class SIM5320CellularContext : public AT_CellularContext, private NonCopyable<SIM5320CellularContext> {
public:
    SIM5320CellularContext(ATHandler &at, SIM5320CellularDevice *device, const char *apn = 0, bool cp_req = false, bool nonip_req = false);
    virtual ~SIM5320CellularContext();

    // AT_CellularContext
    virtual void do_connect() override;
    virtual uint32_t get_timeout_for_operation(ContextOperation op) const override;

    // CellularContext
    virtual nsapi_error_t disconnect() override;
    virtual bool is_connected() override;

protected:
    /**
     * Helper method to call callback function if it is provided
     *
     *  @param status connection status which is parameter in callback function
     */
    void call_connection_status_cb(cellular_connection_status_t status);

    // NetworkInterface interface
    NetworkStack *get_stack() override;

private:
    bool _is_net_opened;
    SIM5320CellularDevice *_sim5320_device;

    /**
     * Check if network is opened.
     *
     * @return
     */
    nsapi_error_t _check_netstate();

    void _urc_netopen();
    void _urc_netclose();
};
}
#endif // SIM5320_CELLULARCONTEXT_H
