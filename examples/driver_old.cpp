#include "sim5320_driver.h"
#include <string.h>
using namespace sim5320;

const int SIM5320::SERIAL_BAUDRATE = 115200;
const int SIM5320::AT_BUFFER_SIZE = 256;
const int SIM5320::AT_TIMEOUT = 8000;
const char* SIM5320::AT_DELIMITER = "\r";

const char* SIM5320::GPS_ASSIST_SERVER_URL = "supl.google.com:7276";
const bool SIM5320::GPS_ASSIST_SERVER_SSL = false;

//
// ATCmdParser note: to match the whole string, it should ends with '\n'
//

SIM5320::SIM5320(UARTSerial* serial_ptr, PinName rts, PinName cts)
    : _serial_ptr(serial_ptr)
    , _parser(serial_ptr, AT_DELIMITER, AT_BUFFER_SIZE, AT_TIMEOUT)
    , _state(0x00)
    , _rts(rts)
    , _cts(cts)
{
    serial_ptr->set_baud(SERIAL_BAUDRATE);
    serial_ptr->set_format(8, UARTSerial::None, 1);
}

SIM5320::SIM5320(PinName tx, PinName rx, PinName rts, PinName cts)
    : _serial_ptr(new UARTSerial(tx, rx, SERIAL_BAUDRATE))
    , _parser(_serial_ptr, AT_DELIMITER, AT_BUFFER_SIZE, AT_TIMEOUT)
    , _state(0x00)
    , _rts(rts)
    , _cts(cts)
{
}

SIM5320::~SIM5320()
{
    if (_state & SIM5320::STATE_CLEANUP_SERIAL) {
        delete _serial_ptr;
    }
}

void SIM5320::debug_on(bool on)
{
    DriverLock(this);
    _parser.debug_on(on);
    if (on) {
        _state |= STATE_DEBUG_ON;
    } else {
        _state &= ~STATE_DEBUG_ON;
    }
}

bool SIM5320::at_available()
{
    DriverLock(this);
    return _parser.send("AT") && _parser.recv("OK\n");
}

bool SIM5320::start_uart_hw_flow_ctrl()
{
    DriverLock(this);
    bool done = true;

    if (_rts != NC && _cts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::RTSCTS, _rts, _cts);
        done = _parser.send("AT+IFC=2,2") && _parser.recv("OK\n");
    } else if (_rts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::RTS, _rts);
        done = _parser.send("AT+IFC=2,0") && _parser.recv("OK\n");
    } else if (_cts != NC) {
        _serial_ptr->set_flow_control(UARTSerial::CTS, _cts);
        done = _parser.send("AT+IFC=0,2") && _parser.recv("OK\n");
    }
}

bool SIM5320::stop_uart_hw_flow_ctrl()
{
    DriverLock(this);
    bool done = true;

    if (_rts != NC || _cts != NC) {
        _serial_ptr->set_flow_control(SerialBase::Disabled, _rts, _cts);
        done = _parser.send("AT+IFC=0,2") && _parser.recv("OK\n");
    }

    return done;
}

int SIM5320::init()
{
    int code;
    bool done;
    DriverLock(this);

    // ensure that serial has correct settings
    _serial_ptr->set_baud(SERIAL_BAUDRATE);
    _serial_ptr->set_format(8, Serial::None, 1);

    // check AT command
    if (!at_available()) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }

    // disable echo
    done = _parser.send("ATE0") && _parser.recv("OK\n");
    if (!done) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }

    // check device model
    char modem_model[24];
    done = _parser.send("AT+CGMM") && _parser.recv("%23s\n", modem_model) && _parser.recv("OK\n");
    if (!done) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }
    if (strncmp(modem_model, "SIMCOM_SIM5320", 14) != 0) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }

    // switch device into low power mode, if
    if (!stop()) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }

    // initialize GPS
    code = gps_init();
    if (code) {
        return code;
    }

    // configure network preferences
    // set network order preference: WCDMA,GSM
    done = _parser.send("AT+CRPAAO=1") && _parser.recv("OK\n");
    //done = done && _parser.send("AT+CNAOP=2") && _parser.recv("OK\n");
    if (!done) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }

    return MBED_SUCCESS;
}

bool SIM5320::reset()
{
    DriverLock(this);
    // close network if it's opened
    _parser.send("AT+NETCLOSE") && _parser.recv("OK\n");

    bool done = _parser.send("AT+CRESET") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // wait reset
    for (int i = 0; i < 10; i++) {
        done = _parser.recv("START\n");
        if (done) {
            break;
        }
    }
    // switch module into minimum functionality mode
    stop();
    // wait end of the SIM and PD initialization
    wait_ms(25000);
    _parser.flush();

    return done;
}

void SIM5320::get_imei(char imei[])
{
    DriverLock(this);
    _parser.send("AT+CGSN") && _parser.recv("%17s\n", imei) && _parser.recv("OK\n");
}

bool SIM5320::start()
{
    DriverLock(this);
    if (!is_active()) {
        bool done = _parser.send("AT+CFUN=1") && _parser.recv("OK\n");
        if (!done) {
            return false;
        }
        // wait end of the SIM and PD initialization
        wait_ms(25000);
        _parser.flush();
    }

    return true;
}

bool SIM5320::stop()
{
    DriverLock(this);
    return _parser.send("AT+CFUN=0") && _parser.recv("OK\n");
}

bool SIM5320::is_active()
{
    DriverLock(this);
    int mode;
    _parser.send("AT+CFUN?");
    _parser.recv("+CFUN: %d\n", &mode) && _parser.recv("OK\n");
    return mode != 0;
}

#define NETWORK_CHECK_DELAY_MS 500

bool SIM5320::network_wait_registration(int timeout)
{
    DriverLock(this);
    Timer timer;
    timer.start();
    while (!network_is_registered()) {
        if (timer.read_ms() > timeout) {
            return false;
        }
        wait_ms(NETWORK_CHECK_DELAY_MS);
    }
    return true;
}

bool SIM5320::network_is_registered()
{
    DriverLock(this);
    int n, stat;

    _parser.send("AT+CREG?");
    _parser.recv("+CREG: %d,%d", &n, &stat) && _parser.recv("OK\n");
    return stat == 1 || stat == 5;
}

float SIM5320::network_get_signal_strength()
{
    DriverLock(this);
    int rssi = 99, ber = 99;
    _parser.send("AT+CSQ") && _parser.recv("+CSQ:%d,%d\n", &rssi, &ber) && _parser.recv("OK\n");
    if (0 <= rssi && rssi < 99) {
        return -113 + 2 * rssi;
    } else {
        return NAN;
    }
}

static SIM5320::NetworkType NETWORK_TYPE_CODE_MAP[] = {
    SIM5320::NETWORK_TYPE_NO_SERVICE,
    SIM5320::NETWORK_TYPE_GSM,
    SIM5320::NETWORK_TYPE_GPRS,
    SIM5320::NETWORK_TYPE_EGPRS,
    SIM5320::NETWORK_TYPE_WCDMA,
    SIM5320::NETWORK_TYPE_HSPA,
    SIM5320::NETWORK_TYPE_HSPA,
    SIM5320::NETWORK_TYPE_HSPA
};

SIM5320::NetworkType SIM5320::network_get_type()
{
    DriverLock(this);
    int n = 0, state = 0;
    _parser.send("AT+CNSMOD?") && _parser.recv("+CNSMOD: %d,%d\n", &n, &state) && _parser.recv("OK\n");
    state = state < 0 ? 0 : state;
    state = state > 7 ? 7 : state;
    return NETWORK_TYPE_CODE_MAP[state];
}

void SIM5320::network_get_operator_name(char name[64])
{
    DriverLock(this);
    int model, format, act;
    bool done;

    done = _parser.send("AT+COPS?") && _parser.recv("+COPS: %d,%d,\"%63[^\"]\",%d\n", &model, &format, name, &act) && _parser.recv("OK\n");
    if (!done) {
        name[0] = '\0';
    }
}

static char* PDP_TYPE_MAP[] = {
    "IP",
    "PPP",
    "IPV6"
};

#define NETWORK_STATUS_CHECK_DELAY 1000
#define NETWORK_STATUS_CHECK_MAX_ATTEMPTS 30

bool SIM5320::network_configure_context(const char* apn, const char* user, const char* password, SIM5320::NetworkDPDType pdp_type)
{
    DriverLock(this);
    bool done;
    // configure PDP context
    done = _parser.send("AT+CGSOCKCONT=%d,\"%s\",\"%s\"", PDP_CONTEXT_NO, PDP_TYPE_MAP[pdp_type], apn) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    done = _parser.send("AT+CSOCKSETPN=%d", PDP_CONTEXT_NO) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // set authentication parameters
    if (user != NULL && password != NULL) {
        done = _parser.send("AT+CSOCKAUTH=%d,1,\"%s\",\"%s\"", PDP_CONTEXT_NO, password, user) && _parser.recv("OK\n");
    } else if (user == NULL && password != NULL) {
        done = _parser.send("AT+CSOCKAUTH=%d,2,\"%s\"", PDP_CONTEXT_NO, password) && _parser.recv("OK\n");
    } else if (user == NULL && password == NULL) {
        done = _parser.send("AT+CSOCKAUTH=%d,0", PDP_CONTEXT_NO) && _parser.recv("OK\n");
    } else {
        MBED_ERROR(MBED_MAKE_ERROR(MBED_ERROR_CODE_INVALID_ARGUMENT, MBED_MODULE_APPLICATION), "Password is NULL and user is not null");
    }
    if (!done) {
        return false;
    }
    // use non-transparent (command) mode
    done = _parser.send("AT+CIPMODE=0") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // open network
    done = _parser.send("AT+NETOPEN") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    int net_state = 0, mode = 0;
    // wait network activation
    for (int i = 0; i < NETWORK_STATUS_CHECK_MAX_ATTEMPTS; i++) {
        done = _parser.send("AT+NETOPEN?") && _parser.recv("+NETOPEN: %d,%d\n", &net_state, &mode) && _parser.recv("OK\n");
        if (!done) {
            return false;
        }
        if (net_state == 1) {
            break;
        }
        wait_ms(NETWORK_STATUS_CHECK_DELAY);
    }
    if (net_state != 1) {
        return false;
    }

    return true;
}

void SIM5320::network_get_device_ip_address(char ip_address[46])
{
    DriverLock(this);
    bool done;
    char buffer[64];

    done = _parser.send("AT+IPADDR") && _parser.recv("%63[^\n]\n", &buffer);
    if (!done) {
        ip_address[0] = '\0';
        return;
    }

    int res = sscanf(buffer, "+IPADDR: %45s", ip_address);
    if (res != 1) {
        ip_address[0] = '\0';
        return;
    }

    // clear other output
    _parser.flush();
}

#define CTRZ_Z_SYM '\x1A'

bool SIM5320::sms_send_message(const char* number, const char* text)
{
    DriverLock(this);
    bool done;

    // set text mode
    done = _parser.send("AT+CMGF=1") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // send sms
    _parser.send("AT+CMGS=\"%s\"", number);
    _parser.recv(">");
    // try to send message even ">" isn't get
    if (_state && STATE_DEBUG_ON) {
        printf("AT MESSAGE> %s\n", text);
    }

    const char* pos = text;
    char sym;
    while ((sym = *(pos++)) != '\0') {
        // replace CTRL+Z to prevent accident message sending
        sym = sym == CTRZ_Z_SYM ? ' ' : sym;
        _parser.putc(sym);
    }
    _parser.putc(CTRZ_Z_SYM); // end of message

    done = _parser.recv("OK\n");
    return done;
}

#define FTP_ACTION_TIMEOUT 32000

int SIM5320::_ftp_read_ret_code(const char* command_name)
{

    char cmd_buff[20];
    char ret_buff[20];
    bool done = false;
    int ret_code = 0;
    sprintf(cmd_buff, "+CFTPS%s: %%s\n", command_name);
    Timer timer;
    timer.start();
    while (!done && timer.read_ms() < FTP_ACTION_TIMEOUT) {
        done = _parser.recv(cmd_buff, ret_buff);
    }
    if (!done) {
        return -1;
    }
    int scan_ret = sscanf(ret_buff, "%d", &ret_code);
    if (scan_ret != 1) {
        return -2;
    }
    return ret_code;
}

bool SIM5320::ftp_connect(const char* host, int port, SIM5320::FTPProtocol protocol, const char* username, const char* password)
{
    DriverLock(this);
    bool done;
    // start ftp stack
    done = _parser.send("AT+CFTPSSTART") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // connect to ftp server
    done = _parser.send("AT+CFTPSLOGIN=\"%s\",%d,\"%s\",\"%s\",%d", host, port, username, password, protocol) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // wait login
    return _ftp_read_ret_code("LOGIN") == 0;
}

bool SIM5320::ftp_get_cwd(char work_dir[256])
{
    DriverLock(this);
    bool done;
    done = _parser.send("AT+CFTPSPWD") && _parser.recv("+CFTPSPWD: \"%255[^\"]\"\n", work_dir) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    return true;
}

bool SIM5320::ftp_set_cwd(const char* work_dir)
{
    DriverLock(this);
    bool done;
    done = _parser.send("AT+CFTPSCWD=\"%s\"", work_dir) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    return true;
}

bool SIM5320::ftp_list_dir(const char* path, Callback<void(const char*)> name_callback)
{
    DriverLock(this);
    bool done;
    int err_code = 0;
    done = _parser.send("AT+CFTPSLIST=\"%s\"", path) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    int data_size;
    done = _parser.recv("+CFTPSLIST: DATA,%d\n", &data_size);
    if (!done) {
        return false;
    }
    // limited ftp list command parsing
    const int max_size = 63;
    int size = 0;
    char buff[max_size + 1];

    for (int i = 0; i < data_size; i++) {
        int c = _parser.getc();
        if (c < 0) {
            return false;
        }
        if (_state & STATE_DEBUG_ON) {
            printf("%c", (char)c);
        }
        // parse output
        switch (c) {
        case ' ':
            size = 0;
            break;
        case '\n':
        case '\r':
            if (size != 0) {
                buff[size] = '\0';
                if (strcmp(buff, ".") != 0 && strcmp(buff, "..") != 0) {
                    name_callback(buff);
                }
                size = 0;
            }
            break;
        default:
            if (size < max_size) {
                buff[size] = c;
                size++;
            }
        }
    }
    return _ftp_read_ret_code("LIST") == 0;
}

bool SIM5320::ftp_exists(const char* path, bool& result)
{
    DriverLock(this);
    bool done;
    char buff[20];
    done = _parser.send("AT+CFTPSSIZE=\"%s\"", path) && _parser.recv("OK\n") && _parser.recv("+CFTPSSIZE: %s\n", &buff);
    if (!done) {
        return false;
    }

    int err_code = 0;
    sscanf(buff, "%d", &err_code);
    return err_code == 0;
}

bool SIM5320::ftp_mkdir(const char* path)
{
    DriverLock(this);
    bool done;
    done = _parser.send("AT+CFTPSMKD=\"%s\"", path) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }

    return true;
}

bool SIM5320::ftp_rmdir(const char* path)
{
    DriverLock(this);
    bool done;
    done = _parser.send("AT+CFTPSRMD=\"%s\"", path) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }

    return true;
}

bool SIM5320::ftp_rmfile(const char* path)
{
    DriverLock(this);
    bool done;
    done = _parser.send("AT+CFTPSDELE=\"%s\"", path) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }

    return true;
}

bool SIM5320::ftp_put(const char* path, Callback<ssize_t(uint8_t*, ssize_t)> data_reader)
{
    return false;
}

bool SIM5320::ftp_get(const char* path, Callback<ssize_t(uint8_t*, ssize_t)> data_writer)
{
    return false;
}

bool SIM5320::ftp_close()
{
    DriverLock(this);
    bool done;
    int ret_code;
    // logout
    done = _parser.send("AT+CFTPSLOGOUT") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    ret_code = _ftp_read_ret_code("LOGOUT");
    if (ret_code != 0) {
        return false;
    }

    // stop ftp stack
    done = _parser.send("AT+CFTPSSTOP") && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    ret_code = _ftp_read_ret_code("STOP");
    if (ret_code != 0) {
        return false;
    }

    return true;
}

int SIM5320::gps_init()
{
    DriverLock(this);
    // stop GPS if it's run
    if (!gps_stop()) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }
    // set AGPS url
    if (!(_parser.send("AT+CGPSURL=\"%s\"", GPS_ASSIST_SERVER_URL) && _parser.recv("OK\n"))) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }
    // set AGPS ssl
    if (!(_parser.send("AT+CGPSSSL=%d", GPS_ASSIST_SERVER_SSL ? 1 : 0) && _parser.recv("OK\n"))) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }
    // disable automatic (AT+CGPSAUTO) AGPS start
    if (!(_parser.send("AT+CGPSAUTO=0") && _parser.recv("OK\n"))) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }
    // set position mode (AT+CGPSPMD) to 127
    if (!(_parser.send("AT+CGPSPMD=127") && _parser.recv("OK\n"))) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }
    // ensure switch to standalone mode automatically
    if (!(_parser.send("AT+CGPSMSB=1") && _parser.recv("OK\n"))) {
        return MBED_ERROR_CODE_INITIALIZATION_FAILED;
    }

    return MBED_SUCCESS;
}

bool SIM5320::gps_start(SIM5320::GPSMode gps_mode)
{
    DriverLock(this);
    int mode;
    if (gps_mode == SIM5320::GPS_STANDALONE_MODE) {
        mode = 1;
    } else {
        mode = 2;
    }
    return _parser.send("AT+CGPS=1,%d", mode) && _parser.recv("OK\n");
}

bool sim5320::SIM5320::gps_stop()
{
    DriverLock(this);
    return _parser.send("AT+CGPS=0") && _parser.recv("OK\n");
}

bool sim5320::SIM5320::gps_is_active()
{
    DriverLock(this);
    int on_flag = 0;
    int mode;
    _parser.send("AT+CGPS?") && _parser.recv("+CGPS:%d,%d\n", &on_flag, &mode) && _parser.recv("OK\n");
    return on_flag;
}

SIM5320::GPSMode SIM5320::get_gps_mode()
{
    DriverLock(this);
    int on_flag;
    int mode = 2;
    _parser.send("AT+CGPS?") && _parser.recv("+CGPS:%d,%d\n", &on_flag, &mode) && _parser.recv("OK\n");
    return mode == 1 ? GPS_STANDALONE_MODE : GPS_UE_BASED_MODE;
}

bool sim5320::SIM5320::gps_get_coord(sim5320::gps_coord_t* coord)
{
    DriverLock(this);
    // read coordinates
    char coord_str[64];
    bool done = _parser.send("AT+CGPSINFO") && _parser.recv("+CGPSINFO:%63s\n", coord_str) && _parser.recv("OK\n");
    if (!done) {
        return false;
    }
    // parse coordinate string
    // example: 3113.343286,N,12121.234064,E,250311,072809.3,44.1,0.0,0
    int lat_deg, lon_deg;
    float lat_min, lon_min;
    char lat_ind, lon_ind;
    tm gps_tm;
    float tm_sec;
    float alt;

    int res = sscanf(coord_str, "%2d%f,%c,%3d%f,%c,%2d%2d%2d,%2d%2d%f,%f",
        &lat_deg, &lat_min, &lat_ind,
        &lon_deg, &lon_min, &lon_ind,
        &gps_tm.tm_mday, &gps_tm.tm_mon, &gps_tm.tm_year,
        &gps_tm.tm_hour, &gps_tm.tm_min, &tm_sec,
        &alt);

    if (res != 13) {
        return false;
    }

    // calculate longitude/latitude
    float lat = lat_deg + lat_min / 60;
    lat = lat_ind == 'S' ? -lat : lat;
    float lon = lon_deg + lon_min / 60;
    lon = lon_ind == 'W' ? -lon : lon;
    coord->latitude = lat;
    coord->longitude = lon;
    coord->altitude = alt;

    // calculate time
    gps_tm.tm_sec = (int)tm_sec;
    gps_tm.tm_year += 100;
    gps_tm.tm_mon -= 1;
    coord->time = mktime(&gps_tm);

    return true;
}

SIM5320::DriverLock::DriverLock(SIM5320* instance_ptr)
    : _instance_ptr(instance_ptr)
{
    instance_ptr->_mutex.lock();
}

SIM5320::DriverLock::~DriverLock()
{
    _instance_ptr->_mutex.unlock();
}
