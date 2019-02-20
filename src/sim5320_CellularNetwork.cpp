#include "sim5320_CellularNetwork.h"
#include "sim5320_CellularStack.h"
#include "sim5320_utils.h"
using namespace sim5320;

SIM5320CellularNetwork::SIM5320CellularNetwork(ATHandler &at_handler)
    : AT_CellularNetwork(at_handler)
{
}

SIM5320CellularNetwork::~SIM5320CellularNetwork()
{
}

nsapi_error_t SIM5320CellularNetwork::set_ciot_optimization_config(CellularNetwork::CIoT_Supported_Opt supported_opt, CellularNetwork::CIoT_Preferred_UE_Opt preferred_opt, Callback<void(CellularNetwork::CIoT_Supported_Opt)> network_support_cb)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SIM5320CellularNetwork::get_ciot_ue_optimization_config(CellularNetwork::CIoT_Supported_Opt &supported_opt, CellularNetwork::CIoT_Preferred_UE_Opt &preferred_opt)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SIM5320CellularNetwork::get_ciot_network_optimization_config(CellularNetwork::CIoT_Supported_Opt &supported_network_opt)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SIM5320CellularNetwork::scan_plmn(CellularNetwork::operList_t &operators, int &ops_count)
{
    _at.lock();
    _at.set_at_timeout(_OPERATORS_SCAN_TIMEOUT);
    nsapi_error_t ret_code = AT_CellularNetwork::scan_plmn(operators, ops_count);
    _at.restore_at_timeout();
    _at.unlock();
    return ret_code;
}

nsapi_error_t SIM5320CellularNetwork::get_operator_names(CellularNetwork::operator_names_list &op_names)
{
    _at.lock();
    _at.set_at_timeout(_OPERATORS_SCAN_TIMEOUT);
    nsapi_error_t ret_code = AT_CellularNetwork::get_operator_names(op_names);
    _at.restore_at_timeout();
    _at.unlock();
    return ret_code;
}

nsapi_error_t SIM5320CellularNetwork::get_registration_params(CellularNetwork::RegistrationType type, CellularNetwork::registration_params_t &reg_params)
{
    if (type == CellularNetwork::C_EREG) {
        return NSAPI_ERROR_UNSUPPORTED;
    }
    nsapi_error_t err = AT_CellularNetwork::get_registration_params(type, reg_params);
    if (err) {
        return err;
    }

    if (reg_params._act == CellularNetwork::RAT_UNKNOWN) {
        CellularNetwork::RadioAccessTechnology act;
        err = get_active_access_technology(act);
        if (err) {
            return err;
        }
        reg_params._act = act;
    }

    return NSAPI_ERROR_OK;
}

static CellularNetwork::RadioAccessTechnology rat_codes[] = {
    CellularNetwork::RAT_UNKNOWN,
    CellularNetwork::RAT_GSM,
    CellularNetwork::RAT_GSM,
    CellularNetwork::RAT_EGPRS,
    CellularNetwork::RAT_UTRAN,
    CellularNetwork::RAT_HSDPA,
    CellularNetwork::RAT_HSUPA,
    CellularNetwork::RAT_HSDPA_HSUPA
};

nsapi_error_t SIM5320CellularNetwork::get_active_access_technology(CellularNetwork::RadioAccessTechnology &op_rat)
{
    int rat_code;
    nsapi_error_t err;

    _at.lock();
    _at.cmd_start("AT+CNSMOD?");
    _at.cmd_stop();
    _at.resp_start("+CNSMOD:");
    _at.skip_param();
    rat_code = _at.read_int();
    err = _at.unlock_return_error();
    if (err) {
        return err;
    }

    if (rat_code < 0 || rat_code >= 8) {
        op_rat = CellularNetwork::RAT_UNKNOWN;
    } else {
        op_rat = rat_codes[rat_code];
    }

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularNetwork::set_preffered_radio_access_technology_mode(SIM5320CellularNetwork::SIM5320PreferredRadioAccessTechnologyMode aop)
{
    _at.lock();
    _at.cmd_start("AT+CNMP=");
    _at.write_int(aop);
    _at.cmd_stop_read_resp();
    return _at.unlock_return_error();
}

nsapi_error_t SIM5320CellularNetwork::set_access_technology_impl(CellularNetwork::RadioAccessTechnology op_rat)
{
    nsapi_error_t err;
    switch (op_rat) {
    case CellularNetwork::RAT_GSM:
    case CellularNetwork::RAT_GSM_COMPACT:
    case CellularNetwork::RAT_EGPRS:
        err = set_preffered_radio_access_technology_mode(PRATM_GSM_ONLY);
        break;

    case mbed::CellularNetwork::RAT_UTRAN:
    case mbed::CellularNetwork::RAT_HSDPA:
    case mbed::CellularNetwork::RAT_HSUPA:
    case mbed::CellularNetwork::RAT_HSDPA_HSUPA:
    case mbed::CellularNetwork::RAT_E_UTRAN:
        err = set_preffered_radio_access_technology_mode(PRATM_WCDMA_ONLY);
        break;
    default:
        err = NSAPI_ERROR_UNSUPPORTED;
    }
    return err;
}
