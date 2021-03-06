#if MBED_CONF_CELLULAR_USE_SMS

#include "mbed_chrono.h"

#include "sim5320_CellularSMS.h"

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using mbed::chrono::milliseconds_u32;
using namespace sim5320;

#define CTRL_Z "\x1a"
#define ESC "\x1b"

void SIM5320CellularSMS::_cmti_urc()
{
    tr_debug("CMTI_URC called");
    //+CMTI: <mem>,<index>,
    // call user defined callback function
    if (_cb) {
        _cb();
    }
}

void SIM5320CellularSMS::_cmt_urc()
{
    tr_debug("CMT_URC called");
    //+CMT: <oa>,[<alpha>],<scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<tosca>,<length>]<CR><LF><data>
    (void)_at.consume_to_stop_tag();
    // call user defined callback function
    if (_cb) {
        _cb();
    }
}

nsapi_error_t SIM5320CellularSMS::get_sms_message_mode(CellularSMS::CellularSMSMmode &mode)
{
    int mode_code = 0;
    int err = _at.at_cmd_int("+CMGF", "?", mode_code);
    mode = mode_code == 1 ? CellularSMSMmodeText : CellularSMSMmodePDU;
    return err;
}

SIM5320CellularSMS::SIM5320CellularSMS(ATHandler &at_handler)
    : _at(at_handler)
{
    // configure SMS callbacks
    _at.set_urc_handler("+CMTI:", callback(this, &SIM5320CellularSMS::_cmti_urc));
    _at.set_urc_handler("+CMT:", callback(this, &SIM5320CellularSMS::_cmt_urc));
}

SIM5320CellularSMS::~SIM5320CellularSMS()
{
    // clear AT callbacks
    _at.set_urc_handler("+CMTI:", nullptr);
    _at.set_urc_handler("+CMT:", nullptr);
}

static const uint8_t FIRST_OCTET_DELIVER_SUBMIT = 17;
static const uint8_t TP_VALIDITY_PERIOD_24_HOURS = 167;
static const uint8_t TP_PROTOCOL_IDENTIFIER = 0;
static const uint8_t SMS_DATA_CODING_SCHEME = 0;

nsapi_error_t SIM5320CellularSMS::initialize(CellularSMS::CellularSMSMmode mode, CellularSMS::CellularSMSEncoding encoding)
{
    if (mode != CellularSMSMmodeText) {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    _use_8bit_encoding = (encoding == CellularSMSEncoding8Bit);

    ATHandlerLocker locker(_at);
    // sms configuration
    _at.at_cmd_discard("+CNMI", "=2,1");
    _at.at_cmd_discard("+CMGF", "=", "%d", CellularSMSMmodeText);
    _at.at_cmd_discard("+CSMP", "=", "%d%d%d%d", FIRST_OCTET_DELIVER_SUBMIT, TP_VALIDITY_PERIOD_24_HOURS, TP_PROTOCOL_IDENTIFIER, SMS_DATA_CODING_SCHEME);
    _at.at_cmd_discard("+CSDH", "=", "%d", 1);

    return _at.get_last_error();
}

static constexpr milliseconds_u32 SMS_CONFIRMATION_TIMEOUT = 12s;

nsapi_size_or_error_t SIM5320CellularSMS::send_sms(const char *phone_number, const char *message, int msg_len)
{
    // the standard AT_CellularSMS:send_sms remove sign +, but it cause error in case of SIM5320
    // so implement SMS sending for text mode
    nsapi_error_t err;
    CellularSMSMmode mode;
    err = get_sms_message_mode(mode);
    if (err) {
        return err;
    }
    if (mode != CellularSMSMmodeText) {
        // don't support non-text mode
        return NSAPI_ERROR_UNSUPPORTED;
    }

    if (msg_len > SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE || !phone_number) {
        return NSAPI_ERROR_PARAMETER;
    }

    ATHandlerLocker locker(_at);

    _at.cmd_start_stop("+CMGS", "=", "%s", phone_number);
    ThisThread::sleep_for(2000ms);
    _at.resp_start("> ", true);

    if (!_at.get_last_error()) {
        int write_size = _at.write_bytes((const uint8_t *)message, msg_len);
        if (write_size < msg_len) {
            // sending can be canceled by giving <ESC> character (IRA 27).
            _at.cmd_start(ESC);
            _at.cmd_stop();
            return write_size;
        }
        // <ctrl-Z> (IRA 26) must be used to indicate the ending of the message body.
        _at.cmd_start(CTRL_Z);
        _at.cmd_stop();
        _at.set_at_timeout(SMS_CONFIRMATION_TIMEOUT);
        _at.resp_start("+CMGS:");
        _at.resp_stop();
        _at.restore_at_timeout();
    }

    err = _at.get_last_error();

    return (err == NSAPI_ERROR_OK) ? msg_len : err;
}

nsapi_size_or_error_t SIM5320CellularSMS::get_sms(char *buf, uint16_t buf_len, char *phone_num, uint16_t phone_len, char *time_stamp, uint16_t time_len, int *buf_size)
{
    CellularSMSMmode mode;
    nsapi_size_or_error_t err;
    int message_no = -1;
    int index;
    char phone_num_tmp[SMS_MAX_PHONE_NUMBER_SIZE];
    char time_stamp_tmp[SMS_MAX_TIME_STAMP_SIZE];
    char message_status[12];
    int message_len = 0;

    // validate buffer sizes already here to avoid any necessary function calls and locking of _at
    if ((phone_num && phone_len < SMS_MAX_PHONE_NUMBER_SIZE) || (time_stamp && time_len < SMS_MAX_TIME_STAMP_SIZE) || buf == NULL) {
        return NSAPI_ERROR_PARAMETER;
    }
    if (buf_len < SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE) {
        *buf_size = SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE;
        return NSAPI_ERROR_NO_MEMORY;
    }

    err = get_sms_message_mode(mode);
    if (err) {
        return err;
    }
    if (mode != CellularSMSMmodeText) {
        // currently we don't support non-text mode
        return NSAPI_ERROR_UNSUPPORTED;
    }

    // list and parse messages
    strcpy(time_stamp, "");
    strcpy(message_status, "");
    strcpy(phone_num_tmp, "");
    ATHandlerLocker locker(_at);
    _at.cmd_start_stop("+CMGL", "=", "%s", "ALL");
    _at.resp_start("+CMGL:");
    while (_at.info_resp()) {
        index = _at.read_int();
        _at.read_string(message_status, 12);
        if (!(strcmp(message_status, "REC UNREAD") == 0 || strcmp(message_status, "REC READ") == 0)) {
            // don't check message of other types
            _at.consume_to_stop_tag();
            _at.consume_to_stop_tag();
        }
        _at.read_string(phone_num_tmp, SMS_MAX_PHONE_NUMBER_SIZE);
        _at.skip_param();
        int len = _at.read_string(time_stamp_tmp, SMS_MAX_TIME_STAMP_SIZE);
        if (len < (SMS_MAX_TIME_STAMP_SIZE - 2)) {
            time_stamp_tmp[len++] = ',';
            _at.read_string(&time_stamp_tmp[len], SMS_MAX_TIME_STAMP_SIZE - len);
        }
        // get value of the last parameter (message length)
        _at.consume_to_stop_tag();

        if (strcmp(time_stamp_tmp, time_stamp) > 0) {
            message_no = index;
            strcpy(time_stamp, time_stamp_tmp);
            strcpy(phone_num, phone_num_tmp);
            // read message
            // TODO: make better sms message processing
            char sym = '\0';
            char prev_sym = '\0';
            int buf_i = 0;
            int read_code = 1;
            while (sym != '\n' && prev_sym != '\r' && read_code == 1) {
                prev_sym = sym;
                read_code = _at.read_bytes((uint8_t *)&sym, 1);
                if (buf_i < buf_len) {
                    buf[buf_i] = sym;
                    buf_i++;
                }
            }
            // add '\0' to buffer
            if (buf_i >= 2 && buf[buf_i - 2] == '\r' && buf[buf_i - 1] == '\n') {
                buf[buf_i - 2] = '\0';
            } else if (buf_i >= buf_len) {
                buf[buf_i - 1] = '\0';
            } else {
                buf[buf_i] = '\0';
            }

        } else {
            _at.consume_to_stop_tag();
        }
    }

    err = _at.get_last_error();
    if (err) {
        return err;
    }
    if (message_no < 0) {
        return -1;
    }

    return message_len;
}

void SIM5320CellularSMS::set_sms_callback(Callback<void()> func)
{
    _cb = func;
}

nsapi_error_t SIM5320CellularSMS::set_cpms(const char *memr, const char *memw, const char *mems)
{
    return _at.at_cmd_discard("+CPMS", "=", "%s%s%s", memr, memw, mems);
}

nsapi_error_t SIM5320CellularSMS::set_csca(const char *sca, int type)
{
    return _at.at_cmd_discard("+CSCA", "=", "%s%d", sca, type);
}

nsapi_size_or_error_t SIM5320CellularSMS::set_cscs(const char *chr_set)
{
    return _at.at_cmd_discard("+CSCS", "=", "%s", chr_set);
}

nsapi_error_t SIM5320CellularSMS::delete_all_messages()
{
    return _at.at_cmd_discard("+CMGD", "=1,4");
}

void SIM5320CellularSMS::set_extra_sim_wait_time(int sim_wait_time)
{
    _sim_wait_time = sim_wait_time;
}

#endif // MBED_CONF_CELLULAR_USE_SMS
