#ifndef SIM5320_CELLULARNETWORK_H
#define SIM5320_CELLULARNETWORK_H
#include "AT_CellularNetwork.h"
#include "mbed.h"
namespace sim5320 {

class SIM5320CellularNetwork : public AT_CellularNetwork, private NonCopyable<SIM5320CellularNetwork> {

public:
    SIM5320CellularNetwork(ATHandler &at_handler);
    virtual ~SIM5320CellularNetwork();

    virtual nsapi_error_t set_ciot_optimization_config(CIoT_Supported_Opt supported_opt,
        CIoT_Preferred_UE_Opt preferred_opt,
        Callback<void(CIoT_Supported_Opt)> network_support_cb);
    virtual nsapi_error_t get_ciot_ue_optimization_config(CIoT_Supported_Opt &supported_opt,
        CIoT_Preferred_UE_Opt &preferred_opt);
    virtual nsapi_error_t get_ciot_network_optimization_config(CIoT_Supported_Opt &supported_network_opt);

    // AT_CellularNetwork
    virtual nsapi_error_t scan_plmn(operList_t &operators, int &ops_count);
    virtual nsapi_error_t get_operator_names(operator_names_list &op_names);
    virtual nsapi_error_t get_registration_params(RegistrationType type, registration_params_t &reg_params);

    /**
     * Get active radio access technology.
     *
     * @param op_rat
     * @return
     */
    virtual nsapi_error_t get_active_access_technology(RadioAccessTechnology &op_rat);

    enum SIM5320PreferredRadioAccessTechnologyMode {
        PRATM_AUTOMATIC = 2,
        PRATM_GSM_ONLY = 13,
        PRATM_WCDMA_ONLY = 14
    };
    /**
     * Set network connection priority.
     *
     * @param aop
     * @return
     */
    virtual nsapi_error_t set_preffered_radio_access_technology_mode(SIM5320PreferredRadioAccessTechnologyMode aop);

protected:
    // AT_CellularNetwork
    virtual nsapi_error_t set_access_technology_impl(RadioAccessTechnology op_rat);

private:
    int _OPERATORS_SCAN_TIMEOUT = 32000;
};
}
#endif // SIM5320_CELLULARNETWORK_H
