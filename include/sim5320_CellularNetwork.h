#ifndef SIM5320_CELLULARNETWORK_H
#define SIM5320_CELLULARNETWORK_H
#include "AT_CellularNetwork.h"
#include "mbed.h"
namespace sim5320 {

class SIM5320CellularNetwork : public AT_CellularNetwork, private NonCopyable<SIM5320CellularNetwork> {

public:
    SIM5320CellularNetwork(ATHandler& atHandler);
    virtual ~SIM5320CellularNetwork();

public: // CellularNetwork
    virtual nsapi_error_t init();

    virtual nsapi_error_t activate_context();

private:
    nsapi_error_t wait_network_status(int expected_status, int timeout);

public:
    virtual nsapi_error_t connect();
    virtual nsapi_error_t disconnect();

    virtual nsapi_error_t get_access_technology(RadioAccessTechnology& rat);

    virtual nsapi_error_t get_registration_status(RegistrationType type, RegistrationStatus& status);

    virtual nsapi_error_t get_extended_signal_quality(int& rxlev, int& ber, int& rscp, int& ecno, int& rsrq, int& rsrp);

    virtual nsapi_error_t set_ciot_optimization_config(Supported_UE_Opt supported_opt, Preferred_UE_Opt preferred_opt);
    virtual nsapi_error_t get_ciot_optimization_config(Supported_UE_Opt& supported_opt, Preferred_UE_Opt& preferred_opt);

protected:
    virtual NetworkStack* get_stack();

    virtual bool get_modem_stack_type(nsapi_ip_stack_t requested_stack);

    virtual nsapi_error_t do_user_authentication();
};
}
#endif // SIM5320_CELLULARNETWORK_H
