#include "sim5320_FTPClient.h"
#include "sim5320_utils.h"
using namespace sim5320;

SIM5320FTPClient::SIM5320FTPClient(ATHandler& at)
    : AT_CellularBase(at)
    , _str_buf(NULL)
    , _last_ftp_error(NSAPI_ERROR_OK)
    , _lock_count(0)
{
}

SIM5320FTPClient::~SIM5320FTPClient()
{
    if (_str_buf) {
        delete[] _str_buf;
    }
}

#define FTP_RESPONSE_TIMEOUT 32000

void SIM5320FTPClient::_lock()
{
    _at.lock();
    _last_ftp_error = NSAPI_ERROR_OK;
    if (_lock_count == 0) {
        _at.set_at_timeout(FTP_RESPONSE_TIMEOUT);
    }
    _lock_count++;
}

void SIM5320FTPClient::_unlock()
{
    _lock_count--;
    if (_lock_count <= 0) {
        _at.restore_at_timeout();
    }
    _at.unlock();
}

nsapi_error_t SIM5320FTPClient::_unlock_return_error()
{
    nsapi_error_t err = _at.get_last_error();
    _unlock();
    return any_error(err, _last_ftp_error);
}

nsapi_error_t SIM5320FTPClient::_get_last_error()
{
    return any_error(_at.get_last_error(), _last_ftp_error);
}

nsapi_error_t SIM5320FTPClient::_get_last_ftp_error()
{
    return _last_ftp_error;
}

void SIM5320FTPClient::_clear_error(bool at_error, bool ftp_error)
{
    if (at_error) {
        _at.clear_error();
    }
    if (ftp_error) {
        _last_ftp_error = NSAPI_ERROR_OK;
    }
}

#define FTP_ERROR_OFFSET -4000

void SIM5320FTPClient::_set_ftp_error_code(int err_code)
{
    if (err_code != 0) {
        if (err_code < 0) {
            _last_ftp_error = FTP_ERROR_OFFSET; // unknown error
        } else {
            _last_ftp_error = FTP_ERROR_OFFSET - err_code;
        }
    } else {
        err_code = NSAPI_ERROR_OK;
    }
}

nsapi_error_t SIM5320FTPClient::_process_ftp_ret_code(const char* command_name)
{
    // clear ftp error
    _clear_error(false, true);
    // return control if ATHandler has error
    if (_at.get_last_error()) {
        return _at.get_last_error();
    }

    // process codes
    char cmd_buff[16];
    sprintf(cmd_buff, "+CFTPS%s:", command_name);
    _at.resp_start(cmd_buff);
    if (_at.get_last_error()) {
        // we got ERROR response
        return _at.get_last_error();
    }
    if (_at.info_resp()) {
        // the FTP code if found first
        _read_ftp_code();
        _at.resp_stop();
    } else {
        // the OK if found first
        _at.resp_start(cmd_buff, false);
        _read_ftp_code();
    }

    return _get_last_error();
}

void SIM5320FTPClient::_read_ftp_code()
{
    if (_at.get_last_error()) {
        return;
    }
    int ftp_err = _at.read_int();
    _set_ftp_error_code(ftp_err);
}

#define _exit_if_error()               \
    if (_get_last_error()) {           \
        return _unlock_return_error(); \
    }

nsapi_error_t SIM5320FTPClient::_init_buffer()
{
    if (_str_buf) {
        return NSAPI_ERROR_OK;
    }
    _str_buf = new char[_STR_BUF_SIZE];
    if (!_str_buf) {
        _set_ftp_error_code(11);
        return NSAPI_ERROR_NO_MEMORY;
    }
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::connect(const char* host, int port, SIM5320FTPClient::FTPProtocol protocol, const char* username, const char* password)
{
    _lock();
    // start ftp stack
    _at.cmd_start("AT+CFTPSSTART");
    _at.cmd_stop();
    _process_ftp_ret_code("START");
    // ignore errors, as stack can be enabled before
    _clear_error();
    // set action timeout
    _at.cmd_start("AT+CFTPSTO=");
    _at.write_int(FTP_RESPONSE_TIMEOUT / 1000 - 1);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    _exit_if_error();
    // connect to server
    _at.cmd_start("AT+CFTPSLOGIN=");
    _at.write_string(host);
    _at.write_int(port);
    _at.write_string(username);
    _at.write_string(password);
    _at.write_int(protocol);
    _at.cmd_stop();
    _process_ftp_ret_code("LOGIN");
    _exit_if_error();
    // set binary transfer type
    _at.cmd_start("AT+CFTPSTYPE=I");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();

    return _unlock_return_error();
}

nsapi_error_t SIM5320FTPClient::get_cwd(char* work_dir, size_t max_size)
{
    _lock();
    _at.cmd_start("AT+CFTPSPWD");
    _at.cmd_stop();
    _at.resp_start("+CFTPSPWD:");
    _at.read_string(work_dir, max_size);
    _at.resp_stop();
    return _unlock_return_error();
}

nsapi_error_t SIM5320FTPClient::set_cwd(const char* work_dir)
{
    _lock();
    _at.cmd_start("AT+CFTPSCWD=");
    _at.write_string(work_dir);
    _at.cmd_stop();
    _at.resp_start("*"); // use fake prefix to reach an ERROR or OK
    _at.resp_stop();
    return _unlock_return_error();
}

nsapi_error_t SIM5320FTPClient::get_file_size(const char* path, long& size)
{
    _lock();
    _at.cmd_start("AT+CFTPSSIZE=");
    _at.write_string(path);
    _at.cmd_stop();
    _at.resp_start("+CFTPSSIZE: ", true);
    _exit_if_error();
    char err_code_str[2];
    _at.read_string(err_code_str, 2);
    if (err_code_str[0] == '0') {
        // consume delimiter
        _at.skip_param();
        // Note: read_int consumes stop tag, if it's last part of answer
        size = _at.read_int();
    } else {
        size = -1;
    }
    // consume ERROR or OK
    _at.resp_start("*", true);
    _clear_error();
    return _unlock_return_error();
}

nsapi_error_t SIM5320FTPClient::isfile(const char* path, bool& result)
{
    long file_size;
    nsapi_error_t err = get_file_size(path, file_size);
    if (err) {
        return err;
    }
    result = file_size >= 0;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::isdir(const char* path, bool& result)
{
    _lock();
    nsapi_error_t err = _init_buffer();
    if (err) {
        return err;
    }
    err = get_cwd(_str_buf, _STR_BUF_SIZE);
    if (err) {
        return err;
    }
    // try to change directory
    err = set_cwd(path);
    if (err) {
        // directory doesn't exists
        result = false;
        _clear_error();
    } else {
        // directory exists
        result = true;
        // return previous directory
        set_cwd(_str_buf);
    }
    return _unlock_return_error();
}

nsapi_error_t SIM5320FTPClient::exists(const char* path, bool& result)
{
    nsapi_error_t err;
    // check if path is file
    err = isfile(path, result);
    if (err) {
        return err;
    }
    if (result) {
        return NSAPI_ERROR_OK;
    }
    // check if path is directory
    err = isdir(path, result);
    if (err) {
        return err;
    }
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::mkdir(const char* path)
{
    _lock();
    _at.cmd_start("AT+CFTPSMKD=");
    _at.write_string(path);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    return _unlock_return_error();
}

nsapi_error_t SIM5320FTPClient::rmdir(const char* path)
{
    _lock();
    _at.cmd_start("AT+CFTPSRMD=");
    _at.write_string(path);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    return _unlock_return_error();
}

bool SIM5320FTPClient::rmfile(const char* path)
{
    _lock();
    _at.cmd_start("AT+CFTPSDELE=");
    _at.write_string(path);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    return _unlock_return_error();
}

#define PUT_UNSEND_MAX 4096
#define PUT_UNSEND_MIN 128
#define FTP_PUT_DATA_WAIT_TIMEOUT 1000
#define FTP_HACK_BLOCK_SIZE 163840
#define FTP_HACK_BLOCK_DELAY 1000

nsapi_error_t SIM5320FTPClient::put(const char* path, Callback<ssize_t(uint8_t*, size_t)> data_writer)
{
    if (!path) {
        return NSAPI_ERROR_PARAMETER;
    }

    _lock();
    _init_buffer();
    _exit_if_error();

    ssize_t block_size = 1;
    int data_writer_error = 0;
    bool data_writer_invalid_ret_val = false;
    int unsent_data = PUT_UNSEND_MAX + 1;

    while (true) {
        // as the operation can be long we should reset ATHanlder timeout
        // TODO: check if it's better to release lock and invoke ThisThread::yield to
        // allow other thread to use this driver.
        _at.lock();
        _at.unlock();

        // check if we have some amount of unsent data
        if (unsent_data >= PUT_UNSEND_MAX) {
            // wait till output buffer become empty almost empty
            while (true) {
                _at.cmd_start("AT+CFTPSPUT?");
                _at.cmd_stop();
                _at.resp_start("+CFTPSPUT:");
                unsent_data = _at.read_int();
                _at.resp_stop();
                // exit if error
                if (_at.get_last_error()) {
                    break;
                }
                if (unsent_data > PUT_UNSEND_MIN) {
                    wait_ms(FTP_PUT_DATA_WAIT_TIMEOUT);
                } else {
                    break;
                }
            }
            // exit if error
            if (_at.get_last_error()) {
                break;
            }
        }

        // get data from user code
        block_size = data_writer((uint8_t*)_str_buf, _STR_BUF_SIZE);
        if (block_size <= 0) {
            // finish transmission
            // if block_size < 0, it will be considered as error code
            data_writer_invalid_ret_val = block_size;
            break;
        } else if (block_size > _STR_BUF_SIZE) {
            // user error
            data_writer_error = NSAPI_ERROR_PARAMETER;
            break;
        }

        // send data
        _at.cmd_start("AT+CFTPSPUT=");
        if (path) {
            _at.write_string(path);
            path = NULL;
        }
        _at.write_int(block_size);
        _at.cmd_stop();

        _at.resp_start(">", true);
        _at.write_bytes((uint8_t*)_str_buf, block_size);

        unsent_data += block_size;
        // get OK confirmation
        _at.resp_start("*", true);
        if (_at.get_last_error()) {
            break;
        }
    }
    // mark that transmission has been finished, even error occurs
    nsapi_error_t err = _get_last_error();
    _at.clear_error();
    _at.cmd_start("AT+CFTPSPUT");
    _at.cmd_stop();
    _process_ftp_ret_code("PUT");

    // process error codes
    err = any_error(err, _unlock_return_error());
    if (data_writer_error < 0) {
        err = any_error(err, data_writer_error);
    }
    return err;
}

#define CACHE_SIZE 1024
#define FTP_DATA_WAIT_TIMEOUT 1000

nsapi_error_t SIM5320FTPClient::get(const char* path, Callback<ssize_t(uint8_t*, size_t)> data_reader)
{
    return NSAPI_ERROR_UNSUPPORTED;
    // TODO: fix get support
    /*
    _at.lock();
    _at.set_at_timeout(FTP_RESPONSE_TIMEOUT);
    uint8_t* cache_buf = new uint8_t[CACHE_SIZE];
    if (!cache_buf) {
        _at.unlock();
        return NSAPI_ERROR_NO_MEMORY;
    }

    // request to get file using cache
    int cftpsget_code = -1;
    ssize_t data_len;
    _at.cmd_start("AT+CFTPSGET=");
    _at.write_string(path);
    _at.write_int(0);
    _at.write_int(1);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();

    // read data from cache
    while (!_at.get_last_error()) {
        _at.cmd_start("AT+CFTPSCACHERD");
        _at.cmd_stop();
        _at.resp_start("+CFTPSGET: ");
        bool has_data = false;
        while (_at.info_resp()) {
            _at.set_stop_tag("\r\n");
            char cftpsget_param[6];
            _at.read_string(cftpsget_param, 6);
            printf("!!! Read: %s\n", cftpsget_param);
            if (strcmp(cftpsget_param, "DATA") == 0) {
                // it's data response
                data_len = _at.read_int();
                // process data

                _at.read_bytes(cache_buf, data_len);
                // TODO: better data processing
                data_reader(cache_buf, data_len);

                printf("!!! receive %d bytes\n", data_len);

                has_data = true;
            } else {
                // we get transmission code
                int code = atoi(cftpsget_param);
                printf("!!! GET URC code %d\n", code);
                cftpsget_code = code < 0 ? 2 : code;
            }
        }

        if (!has_data) {
            if (cftpsget_code < 0) {
                // wait data
                printf("!!! Wait data ...\n");
                wait_ms(FTP_DATA_WAIT_TIMEOUT);
            } else {
                // end of transmission
                printf("!!!Complete\n");
                break;
            }
        }
    }
    _set_ftp_error_code(cftpsget_code);

    _at.restore_at_timeout();
    delete[] cache_buf;
    return _at.unlock_return_error(); */
}

nsapi_error_t SIM5320FTPClient::disconnect()
{
    _lock();

    // disconnect from server
    _at.cmd_start("AT+CFTPSLOGOUT");
    _at.cmd_stop();
    _process_ftp_ret_code("LOGOUT");

    // if any error occurs, ignore it and try to stop ftp stack
    _clear_error();

    // stop stack
    _at.cmd_start("AT+CFTPSSTOP");
    _at.cmd_stop();
    _process_ftp_ret_code("STOP");

    return _unlock_return_error();
}
