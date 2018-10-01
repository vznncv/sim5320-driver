#include "sim5320_FTPClient.h"
#include "sim5320_utils.h"
using namespace sim5320;

static const int FTP_TIMEOUT = 30000;

SIM5320FTPClient::SIM5320FTPClient(ATHandler& at)
    : AT_CellularBase(at)
{
}

SIM5320FTPClient::~SIM5320FTPClient()
{
}

nsapi_error_t SIM5320FTPClient::_read_ftp_ret_code(const char* command_name, bool path_timeout)
{

    if (_at.get_last_error()) {
        return _at.get_last_error();
    }

    if (path_timeout) {
        _at.set_at_timeout(FTP_TIMEOUT);
    }
    char cmd_buff[16];
    sprintf(cmd_buff, "+FTPS%s:", command_name);
    _at.resp_start(cmd_buff, true);
    int err_code = _at.read_int();

    if (path_timeout) {
        _at.restore_at_timeout();
    }

    return any_error(_at.get_last_error(), err_code);
}

nsapi_error_t SIM5320FTPClient::connect(const char* host, int port, SIM5320FTPClient::FTPProtocol protocol, const char* username, const char* password)
{
    int ftp_err = 0;
    _at.lock();
    // start ftp stack
    _at.cmd_start("AT+CFTPSSTART");
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    ftp_err = any_error(ftp_err, _read_ftp_ret_code("START"));

    return any_error(_at.unlock_return_error(), ftp_err);
}

nsapi_error_t SIM5320FTPClient::get_cwd(char work_dir, size_t max_size)
{
}

nsapi_error_t SIM5320FTPClient::set_cwd(const char* work_dir)
{
}

nsapi_error_t SIM5320FTPClient::list_dir(const char* path, Callback<void(const char*)> name_callback)
{
}

nsapi_error_t SIM5320FTPClient::exists(const char* path, bool& result)
{
}

nsapi_error_t SIM5320FTPClient::mkdir(const char* path)
{
}

nsapi_error_t SIM5320FTPClient::rmdir(const char* path)
{
}

bool SIM5320FTPClient::rmfile(const char* path)
{
}

nsapi_error_t SIM5320FTPClient::put(const char* path, Callback<ssize_t(uint8_t*, ssize_t)> data_reader)
{
}

nsapi_error_t SIM5320FTPClient::get(const char* path, Callback<ssize_t(uint8_t*, ssize_t)> data_writer)
{
}

nsapi_error_t SIM5320FTPClient::close()
{
}
