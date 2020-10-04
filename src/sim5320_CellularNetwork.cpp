/**
 * SIM5320 CellularNetwork interface implementation.
 *
 * Partially this code is based on a mbed-os/connectivity/cellular/source/framework/AT/AT_CellularInformation.cpp file code,
 * so it's distributed under original license Apache-2.0 and holds original copyright:
 * - copyright (c) 2017, Arm Limited and affiliates.
 */

#include "CellularContext.h"
#include "CellularUtil.h"

#include "sim5320_CellularNetwork.h"

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using namespace sim5320;
using mbed_cellular_util::hex_str_to_int;

void SIM5320CellularNetwork::_urc_creg()
{
    _read_and_process_reg_params(C_REG);
}

void SIM5320CellularNetwork::_urc_cgreg()
{
    _read_and_process_reg_params(C_GREG);
}

void SIM5320CellularNetwork::_urc_cgev()
{
    if (_connection_status_cb) {
        _connection_status_cb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_DISCONNECTED);
    }
}

nsapi_error_t SIM5320CellularNetwork::_read_reg_params(RegistrationStatus &status, int &lac, int &ci)
{
    const size_t BUF_LEN = 17;
    char buf[BUF_LEN] = { 0 };
    int len;
    int code;

    lac = -1;
    ci = -1;
    code = _at.read_int();
    status = (code >= 0 && code < RegistrationStatusMax) ? (RegistrationStatus)code : NotRegistered;

    // read two bytes location area code in hexadecimal format
    len = _at.read_string(buf, 5);
    if (len > 0) {
        lac = hex_str_to_int(buf, len);
    }

    // read cell ID in hexadecimal format (up to 8 bytes)
    len = _at.read_string(buf, 17);
    if (len > 0) {
        lac = hex_str_to_int(buf, len);
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularNetwork::_read_and_process_reg_params(CellularNetwork::RegistrationType type)
{
    int err;
    RegistrationStatus status;
    int lac;
    int cell_id;
    cell_callback_data_t data;

    err = _read_reg_params(status, lac, cell_id);
    if (err) {
        tr_error("Fail to read registration params");
        return err;
    }

    // process registration status
    if (_reg_params._status != status || _reg_params._type != type) {
        RegistrationStatus previous_registration_status = _reg_params._status;
        _reg_params._status = status;
        _reg_params._type = type;

        if (_connection_status_cb) {
            data.error = NSAPI_ERROR_OK;
            data.status_data = status;
            _connection_status_cb((nsapi_event_t)CellularRegistrationStatusChanged, (intptr_t)&data);
            if (status == NotRegistered) { // Other states means that we are trying to connect or connected
                if (previous_registration_status == RegisteredHomeNetwork || previous_registration_status == RegisteredRoaming) {
                    if (type != C_REG) { // we are interested only if we drop from packet network
                        _connection_status_cb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_DISCONNECTED);
                    }
                }
            }
        }
    }

    // process cell id
    if (cell_id != -1 && cell_id != _reg_params._cell_id) {
        _reg_params._cell_id = cell_id;
        _reg_params._lac = lac;
        if (_connection_status_cb) {
            data.error = NSAPI_ERROR_OK;
            data.status_data = cell_id;
            _connection_status_cb((nsapi_event_t)CellularCellIDChanged, (intptr_t)&data);
        }
    }

    return NSAPI_ERROR_OK;
}

SIM5320CellularNetwork::SIM5320CellularNetwork(ATHandler &at_handler)
    : _at(at_handler)
    , _op_act(RAT_UNKNOWN)
    , _connect_status(NSAPI_STATUS_DISCONNECTED)
{
    // set URC callbacks
    _at.set_urc_handler("+CGREG:", callback(this, &SIM5320CellularNetwork::_urc_cgreg));
    //_at.set_urc_handler("+CREG:", callback(this, &SIM5320CellularNetwork::_urc_creg));
    _at.set_urc_handler("+CGEV: NW DET", callback(this, &SIM5320CellularNetwork::_urc_cgev));
    _at.set_urc_handler("+CGEV: ME DET", callback(this, &SIM5320CellularNetwork::_urc_cgev));
}

SIM5320CellularNetwork::~SIM5320CellularNetwork()
{
    // clear URC callbacks
    _at.set_urc_handler("+CGREG:", nullptr);
    // _at.set_urc_handler("+CREG:", nullptr);
    _at.set_urc_handler("+CGEV: NW DET", nullptr);
    _at.set_urc_handler("+CGEV: ME DET", nullptr);
}

nsapi_error_t SIM5320CellularNetwork::set_registration(const char *plmn)
{
    NWRegisteringMode mode = NWModeAutomatic;

    if (!plmn) {
        tr_debug("Automatic network registration");
        if (get_network_registering_mode(mode) != NSAPI_ERROR_OK) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
        if (mode != NWModeAutomatic) {
            return _at.at_cmd_discard("+COPS", "=0");
        }
        return NSAPI_ERROR_OK;
    } else {
        tr_debug("Manual network registration to %s", plmn);
        mode = NWModeManual;
        OperatorNameFormat format = OperatorNameNumeric;
#ifdef MBED_CONF_CELLULAR_PLMN_FALLBACK_AUTO
        mode = NWModeManualAutomatic;
#endif

        return _at.at_cmd_discard("+COPS", "=", "%d%d%s", mode, format, plmn);
    }
}

nsapi_error_t SIM5320CellularNetwork::get_network_registering_mode(CellularNetwork::NWRegisteringMode &mode)
{
    int ret;
    nsapi_error_t error = _at.at_cmd_int("+COPS", "?", ret);
    if (error == NSAPI_ERROR_OK) {
        mode = (NWRegisteringMode)ret;
    }
    return error;
}

nsapi_error_t SIM5320CellularNetwork::set_registration_urc(CellularNetwork::RegistrationType type, bool on)
{
    int mode = 0;
    if (on) {
        mode = 2;
    }

    switch (type) {
    case mbed::CellularNetwork::C_GREG:
        return _at.at_cmd_discard("+CGREG", "=", "%d", mode);
        break;
        //    case mbed::CellularNetwork::C_REG:
        //        return _at.at_cmd_discard("+CREG", "=", "%d", mode);
        //        break;
    default:
        return NSAPI_ERROR_UNSUPPORTED;
    }
}

nsapi_error_t SIM5320CellularNetwork::set_attach()
{
    ATHandlerLocker locker(_at);
    AttachStatus status;
    get_attach(status);

    if (status == Detached) {
        tr_debug("Network attach");
        _at.at_cmd_discard("+CGATT", "=1");
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularNetwork::get_attach(CellularNetwork::AttachStatus &status)
{
    int attach_status;
    nsapi_error_t err = _at.at_cmd_int("+CGATT", "?", attach_status);
    status = (attach_status == 1) ? Attached : Detached;

    return err;
}

#define SIM5320_DETACH_TIMEOUT 16000

nsapi_error_t SIM5320CellularNetwork::detach()
{
    // note: add extra timeout for this operation
    ATHandlerLocker locker(_at, SIM5320_DETACH_TIMEOUT);

    tr_debug("Network detach");
    _at.at_cmd_discard("+CGATT", "=0");
    _at.at_cmd_discard("+COPS", "=2");

    if (_connection_status_cb) {
        _connection_status_cb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_DISCONNECTED);
    }

    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularNetwork::set_access_technology(CellularNetwork::RadioAccessTechnology rat)
{
    nsapi_error_t err;
    switch (rat) {
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

    if (!err) {
        _op_act = rat;
    }

    return err;
}

#define SIM5320_OPERATORS_SCAN_TIMEOUT 120000

nsapi_error_t SIM5320CellularNetwork::scan_plmn(CellularNetwork::operList_t &operators, int &ops_count)
{
    ATHandlerLocker locker(_at, SIM5320_OPERATORS_SCAN_TIMEOUT);
    int idx = 0;

    _at.cmd_start_stop("+COPS", "=?");
    _at.resp_start("+COPS:");

    int ret, error_code = -1;
    operator_t *op = NULL;

    while (_at.info_elem('(')) {

        op = operators.add_new();
        op->op_status = (operator_t::Status)_at.read_int();
        _at.read_string(op->op_long, sizeof(op->op_long));
        _at.read_string(op->op_short, sizeof(op->op_short));
        _at.read_string(op->op_num, sizeof(op->op_num));

        // Optional - try read an int
        ret = _at.read_int();
        op->op_rat = (ret == error_code) ? RAT_UNKNOWN : (RadioAccessTechnology)ret;

        if ((_op_act == RAT_UNKNOWN) || ((op->op_rat != RAT_UNKNOWN) && (op->op_rat == _op_act))) {
            idx++;
        } else {
            operators.delete_last();
        }
    }
    _at.resp_stop();

    ops_count = idx;
    return _at.get_last_error();
}

nsapi_error_t SIM5320CellularNetwork::set_ciot_optimization_config(CellularNetwork::CIoT_Supported_Opt supported_opt, CellularNetwork::CIoT_Preferred_UE_Opt preferred_opt, Callback<void(CellularNetwork::CIoT_Supported_Opt)> network_support_cb)
{
    return NSAPI_ERROR_DEVICE_ERROR;
}

nsapi_error_t SIM5320CellularNetwork::get_ciot_ue_optimization_config(CellularNetwork::CIoT_Supported_Opt &supported_opt, CellularNetwork::CIoT_Preferred_UE_Opt &preferred_opt)
{
    supported_opt = CIOT_OPT_NO_SUPPORT;
    preferred_opt = PREFERRED_UE_OPT_NO_PREFERENCE;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularNetwork::get_ciot_network_optimization_config(CellularNetwork::CIoT_Supported_Opt &supported_network_opt)
{
    supported_network_opt = CIOT_OPT_NO_SUPPORT;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularNetwork::get_signal_quality(int &rssi, int *ber)
{
    ATHandlerLocker locker(_at);

    _at.cmd_start_stop("+CSQ", "");

    _at.resp_start("+CSQ:");
    int t_rssi = _at.read_int();
    int t_ber = _at.read_int();
    _at.resp_stop();
    if (t_rssi < 0 || t_ber < 0) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    // RSSI value is returned in dBm with range from -51 to -113 dBm, see 3GPP TS 27.007
    if (t_rssi == 99) {
        rssi = SignalQualityUnknown;
    } else {
        rssi = -113 + 2 * t_rssi;
    }

    if (ber) {
        if (t_ber == 99) {
            *ber = SignalQualityUnknown;
        } else {
            *ber = t_ber;
        }
    }

    return _at.get_last_error();
}

int SIM5320CellularNetwork::get_3gpp_error()
{
    return _at.get_3gpp_error();
}

nsapi_error_t SIM5320CellularNetwork::get_operator_params(int &format, CellularNetwork::operator_t &operator_params)
{
    ATHandlerLocker locker(_at);

    _at.cmd_start_stop("+COPS", "?");

    _at.resp_start("+COPS:");
    _at.read_int(); //ignore mode
    format = _at.read_int();

    if (_at.get_last_error() == NSAPI_ERROR_OK) {
        switch (format) {
        case 0:
            _at.read_string(operator_params.op_long, sizeof(operator_params.op_long));
            break;
        case 1:
            _at.read_string(operator_params.op_short, sizeof(operator_params.op_short));
            break;
        default:
            _at.read_string(operator_params.op_num, sizeof(operator_params.op_num));
            break;
        }
        operator_params.op_rat = (RadioAccessTechnology)_at.read_int();
    }

    _at.resp_stop();

    return _at.get_last_error();
}

void SIM5320CellularNetwork::attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    _connection_status_cb = status_cb;
}

nsapi_error_t SIM5320CellularNetwork::get_operator_names(CellularNetwork::operator_names_list &op_names)
{
    ATHandlerLocker locker(_at);

    _at.cmd_start_stop("+COPN", "");

    _at.resp_start("+COPN:");
    operator_names_t *names = NULL;
    while (_at.info_resp()) {
        names = op_names.add_new();
        _at.read_string(names->numeric, sizeof(names->numeric));
        _at.read_string(names->alpha, sizeof(names->alpha));
    }

    _at.resp_stop();
    return _at.get_last_error();
}

bool SIM5320CellularNetwork::is_active_context(int *number_of_active_contexts, int cid)
{
    ATHandlerLocker locker(_at);

    if (number_of_active_contexts) {
        *number_of_active_contexts = 0;
    }
    bool active_found = false;
    int context_id;

    // read active contexts
    _at.cmd_start_stop("+CGACT", "?");
    _at.resp_start("+CGACT:");
    while (_at.info_resp()) {
        context_id = _at.read_int(); // discard context id
        if (_at.read_int() == 1) { // check state
            tr_debug("Found active context");
            if (number_of_active_contexts) {
                (*number_of_active_contexts)++;
            }
            if (cid == -1) {
                active_found = true;
            } else if (context_id == cid) {
                active_found = true;
            }
            if (!number_of_active_contexts && active_found) {
                break;
            }
        }
    }
    _at.resp_stop();

    return active_found;
}

nsapi_error_t SIM5320CellularNetwork::get_registration_params(CellularNetwork::registration_params_t &reg_params)
{
    reg_params = _reg_params;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularNetwork::get_registration_params(CellularNetwork::RegistrationType type, CellularNetwork::registration_params_t &reg_params)
{
    const char *cmd;
    int err;
    const char *resp_prefix;
    RegistrationStatus status;
    RadioAccessTechnology rat;
    int cell_id;
    int lac;

    switch (type) {
    case mbed::CellularNetwork::C_GREG:
        cmd = "+CGREG";
        resp_prefix = "+CGREG:";
        break;
        //    case mbed::CellularNetwork::C_REG:
        //        cmd = "+CREG";
        //        resp_prefix = "+CREG:";
        //        break;
    default:
        return NSAPI_ERROR_UNSUPPORTED;
    }

    {
        ATHandlerLocker locker(_at);
        // get registration status
        _at.cmd_start_stop(cmd, "?");
        _at.resp_start(resp_prefix);
        (void)_at.skip_param(); // ignore urc mode subparam
        _read_reg_params(status, lac, cell_id);
        _at.resp_stop();
        // get network technology
        get_active_access_technology(rat);
        err = _at.get_last_error();
    }
    if (err) {
        return err;
    }

    _reg_params._type = type;
    _reg_params._status = status;
    _reg_params._cell_id = cell_id;
    _reg_params._lac = lac;
    _reg_params._act = rat;
    reg_params = _reg_params;

    return err;
}

nsapi_error_t SIM5320CellularNetwork::set_receive_period(int mode, CellularNetwork::EDRXAccessTechnology act_type, uint8_t edrx_value)
{
    return NSAPI_ERROR_UNSUPPORTED;
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

nsapi_error_t SIM5320CellularNetwork::set_preffered_radio_access_technology_mode(SIM5320CellularNetwork::SIM5320PreferredRadioAccessTechnologyMode aop)
{
    return _at.at_cmd_discard("+CNMP", "=", "%d", (int)aop);
}

nsapi_error_t SIM5320CellularNetwork::get_active_access_technology(CellularNetwork::RadioAccessTechnology &op_rat)
{
    int rat_code;
    nsapi_error_t err;
    ATHandlerLocker locker(_at);

    _at.cmd_start_stop("+CNSMOD", "?");
    _at.resp_start("+CNSMOD:");
    _at.skip_param();
    rat_code = _at.read_int();
    _at.resp_stop();
    err = _at.get_last_error();
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
