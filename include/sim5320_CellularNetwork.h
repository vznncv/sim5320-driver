#ifndef SIM5320_CELLULARNETWORK_H
#define SIM5320_CELLULARNETWORK_H

#include "mbed.h"

#include "ATHandler.h"
#include "CellularNetwork.h"

namespace sim5320 {

/**
 * SIM5320 cellular network implementation.
 */
class SIM5320CellularNetwork : public CellularNetwork, private NonCopyable<SIM5320CellularNetwork> {
protected:
    ATHandler &_at;

    Callback<void(nsapi_event_t, intptr_t)> _connection_status_cb;
    RadioAccessTechnology _op_act;
    nsapi_connection_status_t _connect_status;

    registration_params_t _reg_params;

private:
    // URC callbacks
    void _urc_creg();
    void _urc_cgreg();
    void _urc_cgev();

    nsapi_error_t _read_reg_params(RegistrationStatus &status, int &lac, int &ci);
    nsapi_error_t _read_and_process_reg_params(RegistrationType type);

public:
    SIM5320CellularNetwork(ATHandler &at_handler);
    virtual ~SIM5320CellularNetwork();

    // CellularNetwork interface
public:
    nsapi_error_t set_registration(const char *plmn = 0) override;
    nsapi_error_t get_network_registering_mode(NWRegisteringMode &mode) override;
    nsapi_error_t set_registration_urc(RegistrationType type, bool on) override;
    nsapi_error_t set_attach() override;
    nsapi_error_t get_attach(AttachStatus &status) override;
    nsapi_error_t detach() override;
    nsapi_error_t set_access_technology(RadioAccessTechnology rat) override;
    nsapi_error_t scan_plmn(operList_t &operators, int &ops_count) override;
    nsapi_error_t set_ciot_optimization_config(CIoT_Supported_Opt supported_opt, CIoT_Preferred_UE_Opt preferred_opt, Callback<void(CIoT_Supported_Opt)> network_support_cb) override;
    nsapi_error_t get_ciot_ue_optimization_config(CIoT_Supported_Opt &supported_opt, CIoT_Preferred_UE_Opt &preferred_opt) override;
    nsapi_error_t get_ciot_network_optimization_config(CIoT_Supported_Opt &supported_network_opt) override;
    nsapi_error_t get_signal_quality(int &rssi, int *ber) override;
    int get_3gpp_error() override;
    nsapi_error_t get_operator_params(int &format, operator_t &operator_params) override;
    void attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb) override;
    nsapi_error_t get_operator_names(operator_names_list &op_names) override;
    bool is_active_context(int *number_of_active_contexts, int cid) override;
    nsapi_error_t get_registration_params(registration_params_t &reg_params) override;
    nsapi_error_t get_registration_params(RegistrationType type, registration_params_t &reg_params) override;
    nsapi_error_t set_receive_period(int mode, EDRXAccessTechnology act_type, uint8_t edrx_value) override;

public:
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

    /**
     * Show current radio access technology.
     *
     * @param op_rat
     * @return
     */
    nsapi_error_t get_active_access_technology(CellularNetwork::RadioAccessTechnology &op_rat);

private:
};
}
#endif // SIM5320_CELLULARNETWORK_H
