#include "sim5320_FTPClient.h"

#include "mbed_chrono.h"

#include <chrono>
#include <string.h>

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using mbed::chrono::milliseconds_u32;
using namespace sim5320;

static constexpr milliseconds_u32 FTP_RESPONSE_TIMEOUT = 24s;
static constexpr int FTP_DEVICE_TIMEOUT = 20;

SIM5320FTPClient::SIM5320FTPClient(ATHandler &at)
    : _at(at)
    , _buffer(nullptr)
    , _cleanup_buffer(false)
{
}

SIM5320FTPClient::~SIM5320FTPClient()
{
    if (_cleanup_buffer) {
        delete[] _buffer;
    }
}

char *SIM5320FTPClient::_get_buffer()
{
    if (!_buffer) {
        _cleanup_buffer = true;
        _buffer = new char[BUFFER_SIZE];
    }
    return _buffer;
}

nsapi_error_t SIM5320FTPClient::set_buffer(uint8_t *buf)
{
    if (_buffer) {
        return MBED_ERROR_CODE_ALREADY_INITIALIZED;
    } else {
        _buffer = (char *)buf;
        return NSAPI_ERROR_OK;
    }
}

#define FTP_ERROR_OFFSET -4000

static int convert_ftp_error_code(int cmd_code)
{
    if (cmd_code == 0) {
        return 0;
    } else if (cmd_code < 0) {
        return FTP_ERROR_OFFSET;
    } else {
        return FTP_ERROR_OFFSET - cmd_code;
    }
}

static int read_fuzzy_ftp_response(ATHandler &at, bool wait_response_after_ok, bool wait_response_after_error, const char *prefix)
{
    int err;
    int ftp_code;
    size_t len;
    char full_prefix[16];

    strcpy(full_prefix, prefix);
    len = strlen(prefix);
    full_prefix[len] = ':';
    full_prefix[len] = '\0';

    err = read_full_fuzzy_response(at, wait_response_after_ok, wait_response_after_error, full_prefix, "%i", &ftp_code);
    if (err >= 1) {
        return convert_ftp_error_code(ftp_code);
    } else if (err == 0) {
        if (wait_response_after_ok) {
            return convert_ftp_error_code(0);
        } else {
            return 0;
        }
    } else {
        return err;
    }
}

#define _RETURN_IF_FTP_ERROR(err, ftp_code)      \
    if (err) {                                   \
        return err;                              \
    }                                            \
    if (ftp_code) {                              \
        return convert_ftp_error_code(ftp_code); \
    }

#define RETURN_IF_ERROR(err) \
    if (err) {               \
        return err;          \
    }

nsapi_error_t SIM5320FTPClient::connect(const char *host, int port, SIM5320FTPClient::FTPProtocol protocol, const char *username, const char *password)
{
    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    // start ftp stack
    _at.cmd_start_stop("+CFTPSSTART", "", "");
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSSTART");
    if (err) {
        // try to stop and start stack again
        _at.clear_error();
        _at.cmd_start_stop("+CFTPSSTOP", "", "");
        read_fuzzy_ftp_response(_at, false, false, "+CFTPSSTOP");
        _at.clear_error();
        _at.cmd_start_stop("+CFTPSSTART", "", "");

        read_fuzzy_ftp_response(_at, false, false, "+CFTPSSTART");
    }
    _at.clear_error(); // ignore errors

    // set action timeout
    _at.at_cmd_discard("+CFTPSTO", "=", "%d", FTP_DEVICE_TIMEOUT);
    _at.clear_error();

    // connect to server
    _at.cmd_start_stop("+CFTPSLOGIN", "=", "%s%d%s%s%d", host, port, username, password, protocol);
    err = read_fuzzy_ftp_response(_at, true, false, "+CFTPSLOGIN");
    RETURN_IF_ERROR(err);

    // set binary transfer type
    _at.cmd_start_stop("+CFTPSTYPE", "=I");
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSTYPE");
    RETURN_IF_ERROR(err);

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::connect(const char *address)
{
    const size_t max_address_len = 79;
    char tmp_address[max_address_len + 1];
    if (strlen(address) >= max_address_len) {
        return NSAPI_ERROR_PARAMETER;
    }
    strcpy(tmp_address, address);
    char *address_pos = tmp_address;
    char *at_sign_pos, *column_pos;
    FTPProtocol protocol;
    const char *username;
    const char *password;
    const char *host;
    int port;

    // parse protocol
    if (strncmp("ftp://", tmp_address, 6) == 0) {
        protocol = FTP;
        address_pos += 6;
    } else if (strncmp("ftps://", tmp_address, 7) == 0) {
        protocol = FTPS_EXPLICIT;
        address_pos += 7;
    } else if (strncmp("ftps+e://", tmp_address, 9) == 0) {
        protocol = FTPS_EXPLICIT;
        address_pos += 9;
    } else if (strncmp("ftps+i://", tmp_address, 9) == 0) {
        protocol = FTPS_IMPLICIT;
        address_pos += 9;
    } else {
        // invalid protocol
        return NSAPI_ERROR_PARAMETER;
    }

    // parse username and password
    at_sign_pos = strchr(address_pos, '@');
    if (at_sign_pos != nullptr) {
        // parse user and password
        *at_sign_pos = '\0';
        column_pos = strchr(address_pos, ':');
        if (column_pos == nullptr) {
            return NSAPI_ERROR_PARAMETER;
        }
        *column_pos = '\0';
        username = address_pos;
        password = column_pos + 1;
        address_pos = at_sign_pos + 1;
    } else {
        // use anonymous user
        username = "anonymous";
        password = "";
    }

    // parse host and port
    column_pos = strchr(address_pos, ':');
    if (column_pos == nullptr) {
        // use default port
        host = address_pos;
        port = 21;
    } else {
        *column_pos = '\0';
        host = address_pos;
        address_pos = column_pos + 1;
        port = atoi(address_pos);
        if (port == 0) {
            return NSAPI_ERROR_PARAMETER;
        }
    }

    return connect(host, port, protocol, username, password);
}

nsapi_error_t SIM5320FTPClient::disconnect()
{
    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    // disconnect from server
    _at.cmd_start_stop("+CFTPSLOGOUT", "");
    read_fuzzy_ftp_response(_at, true, false, "+CFTPSLOGOUT");
    _at.clear_error(); // if any error occurs, ignore it and try to stop ftp stack

    // stop stack
    _at.cmd_start_stop("+CFTPSSTOP", "");
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSSTOP");

    return err;
}

nsapi_error_t SIM5320FTPClient::get_cwd(char *work_dir, size_t max_size)
{
    return _at.at_cmd_str("+CFTPSPWD", "", work_dir, max_size);
}

nsapi_error_t SIM5320FTPClient::set_cwd(const char *work_dir)
{
    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    _at.cmd_start_stop("+CFTPSCWD", "=", "%s", work_dir);
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSCWD");
    RETURN_IF_ERROR(err);

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::get_file_size(const char *path, long &size)
{
    int err, ftp_code;
    int cmd_fsize;

    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);
    _at.cmd_start_stop("+CFTPSSIZE", "=", "%s", path);
    err = read_full_fuzzy_response(_at, false, false, "+CFTPSSIZE:", "%i%i", &ftp_code, &cmd_fsize);

    if (ftp_code == 0 && err == 2) {
        size = cmd_fsize;
    } else {
        size = -1;
    }
    _at.clear_error();
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::isfile(const char *path, bool &result)
{
    long file_size;
    nsapi_error_t err = get_file_size(path, file_size);
    if (err) {
        return err;
    }
    result = file_size >= 0;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::isdir(const char *path, bool &result)
{
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);
    int err;
    char *buf = _get_buffer();

    err = get_cwd(buf, BUFFER_SIZE);
    if (err) {
        return err;
    }
    // try to change directory
    err = set_cwd(path);
    if (err) {
        // directory doesn't exists
        result = false;
        _at.clear_error();
    } else {
        // directory exists
        result = true;
        // return previous directory
        set_cwd(buf);
    }
    return _at.get_last_error();
}

nsapi_error_t SIM5320FTPClient::exists(const char *path, bool &result)
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

nsapi_error_t SIM5320FTPClient::mkdir(const char *path)
{
    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    _at.cmd_start_stop("+CFTPSMKD", "=", "%s", path);
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSMKD");
    RETURN_IF_ERROR(err);

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::rmdir(const char *path)
{
    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    _at.cmd_start_stop("+CFTPSRMD", "=", "%s", path);
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSRMD");
    RETURN_IF_ERROR(err);

    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320FTPClient::_rmtree_impl(char *path_buf, size_t path_buf_len)
{
    int err;
    SIM5320FTPClient::dir_entry_list_t dir_entry_list;
    SIM5320FTPClient::dir_entry_t *dir_entry_ptr;
    size_t path_len = strlen(path_buf) + 1;

    err = listdir(path_buf, &dir_entry_list);
    if (err) {
        return err;
    }
    for (dir_entry_ptr = dir_entry_list.get_head(); dir_entry_ptr != nullptr; dir_entry_ptr = dir_entry_ptr->next) {
        size_t new_path_len = path_len + 1 + strlen(dir_entry_ptr->name);
        if (new_path_len >= path_buf_len) {
            err = MBED_ERROR_CODE_INVALID_SIZE;
            break;
        }

        path_buf[path_len - 1] = '/';
        strcpy(path_buf + path_len, dir_entry_ptr->name);
        if (dir_entry_ptr->d_type == DT_REG) {
            err = rmfile(path_buf);
        } else {
            err = _rmtree_impl(path_buf, path_buf_len);
            if (!err) {
                err = rmdir(path_buf);
            }
        }

        if (err) {
            break;
        }
    }
    path_buf[path_len - 1] = '\0';
    return err;
}

nsapi_error_t SIM5320FTPClient::rmtree(const char *path, bool remove_root)
{
    const size_t path_buf_len = 256;
    if (strlen(path) > path_buf_len) {
        return MBED_ERROR_INVALID_SIZE;
    }
    char *path_buf = new char[path_buf_len];
    strcpy(path_buf, path);
    int err = _rmtree_impl(path_buf, path_buf_len);
    if (!err && remove_root) {
        err = rmdir(path);
    }
    delete[] path_buf;
    return err;
}

nsapi_error_t SIM5320FTPClient::rmfile(const char *path)
{
    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    _at.cmd_start_stop("+CFTPSDELE", "=", "%s", path);
    err = read_fuzzy_ftp_response(_at, false, false, "+CFTPSDELE");
    RETURN_IF_ERROR(err);

    return NSAPI_ERROR_OK;
}

SIM5320FTPClient::dir_entry_t::dir_entry_t()
    : name(nullptr)
    , d_type(DT_UNKNOWN)
{
}

SIM5320FTPClient::dir_entry_t::~dir_entry_t()
{
    if (name) {
        delete[] name;
    }
}

namespace sim5320 {
struct listdir_callback_t {
    SIM5320FTPClient::dir_entry_list_t *dir_entry_list;
    static const size_t MAX_WORD_SIZE = 63;
    char word_buf[MAX_WORD_SIZE];
    size_t word_i;
    char current_d_type;
    bool line_start;

    ssize_t process(uint8_t *buf, size_t len)
    {
        for (size_t i = 0; i < len; i++) {
            char sym = buf[i];
            if (sym == ' ') {
                // check <DIR> and skip word
                if (word_i >= 5 && current_d_type == DT_UNKNOWN) {
                    if (strncmp(word_buf, "<DIR>", 5) == 0) {
                        current_d_type = DT_DIR;
                    }
                }
                word_i = 0;
            } else if (sym == '\n' || sym == '\r') {
                // ignore multiple '\n' and '\r'
                if (!line_start) {
                    // assume that last word is filename
                    SIM5320FTPClient::dir_entry_t *dir_entry_ptr = dir_entry_list->add_new();
                    dir_entry_ptr->d_type = current_d_type != DT_UNKNOWN ? current_d_type : DT_REG;
                    dir_entry_ptr->name = new char[word_i + 1];
                    strncpy(dir_entry_ptr->name, word_buf, word_i);
                    dir_entry_ptr->name[word_i] = '\0';
                    line_start = true;
                    current_d_type = DT_UNKNOWN;
                    word_i = 0;
                }
            } else {
                if (line_start) {
                    // check entity type
                    if (sym == 'd') {
                        current_d_type = DT_DIR;
                    } else if (sym == '-') {
                        current_d_type = DT_REG;
                    }
                    // else: it's probably window output, so try to find <DIR> later
                    line_start = false;
                }

                // add symbol to current word
                if (word_i < MAX_WORD_SIZE) {
                    word_buf[word_i] = sym;
                    word_i++;
                }
            }
        }

        return len;
    }
};
}

nsapi_error_t SIM5320FTPClient::listdir(const char *path, SIM5320FTPClient::dir_entry_list_t *dir_entry_list)
{
    listdir_callback_t listdir_callback;
    listdir_callback.current_d_type = DT_UNKNOWN;
    listdir_callback.dir_entry_list = dir_entry_list;
    listdir_callback.line_start = true;
    listdir_callback.word_i = 0;
    return _get_data_impl(path, callback(&listdir_callback, &listdir_callback_t::process), "LIST");
}

#define PUT_UNSEND_MAX 6144
#define PUT_UNSEND_MIN 2048
static constexpr size_t FTP_PUT_DATA_WAIT_TIMEOUT_SCHEME_SIZE = 8;
static constexpr milliseconds_u32 FTP_PUT_DATA_WAIT_TIMEOUT_SCHEME[FTP_PUT_DATA_WAIT_TIMEOUT_SCHEME_SIZE] = { 1ms, 5ms, 20ms, 50ms, 100ms, 200ms, 500ms, 1000ms };
#define FTP_HACK_BLOCK_SIZE 163840
#define FTP_HACK_BLOCK_DELAY 1000

nsapi_error_t SIM5320FTPClient::put(const char *path, Callback<ssize_t(uint8_t *, size_t)> data_writer)
{
    if (!path) {
        return NSAPI_ERROR_PARAMETER;
    }

    int err;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    char *buf = _get_buffer();

    ssize_t block_size = 1;
    ssize_t total_size = 0;
    int data_writer_error = 0;
    int pending_data_i = PUT_UNSEND_MAX + 1;
    size_t put_wait_timeout_scheme_i;

    while (true) {
        // as the operation can be long we should reset ATHanlder timeout
        // TODO: check if it's better to release lock and invoke ThisThread::yield to
        // allow other thread to use this driver.
        locker.reset_timeout();

        // check if we have some amount of unsent data
        if (pending_data_i >= PUT_UNSEND_MAX) {
            // wait till output buffer become empty almost empty
            put_wait_timeout_scheme_i = 0;
            while (true) {
                _at.at_cmd_int("+CFTPSPUT", "?", pending_data_i, "");
                // exit if error
                if (_at.get_last_error()) {
                    break;
                }
                if (pending_data_i > PUT_UNSEND_MIN) {
                    ThisThread::sleep_for(FTP_PUT_DATA_WAIT_TIMEOUT_SCHEME[put_wait_timeout_scheme_i]);
                    if (put_wait_timeout_scheme_i < (FTP_PUT_DATA_WAIT_TIMEOUT_SCHEME_SIZE + 1)) {
                        put_wait_timeout_scheme_i++;
                    }
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
        block_size = data_writer((uint8_t *)buf, BUFFER_SIZE);
        if (block_size <= 0) {
            // finish transmission
            // if block_size < 0, it will be considered as error code
            data_writer_error = block_size;
            break;
        } else if ((size_t)block_size > BUFFER_SIZE) {
            // user error
            data_writer_error = NSAPI_ERROR_PARAMETER;
            break;
        }
        total_size += block_size;

        // send data
        if (path) {
            _at.cmd_start_stop("+CFTPSPUT", "=", "%s%d", path, block_size);
            path = nullptr;
        } else {
            _at.cmd_start_stop("+CFTPSPUT", "=", "%d", block_size);
        }

        _at.resp_start(">", true);
        _at.write_bytes((uint8_t *)buf, block_size);

        pending_data_i += block_size;
        // get OK confirmation
        _at.resp_start("*", true);
        if (_at.get_last_error()) {
            break;
        }
    }
    // mark that transmission has been finished, even error occurs
    err = _at.get_last_error();
    _at.clear_error();

    if (total_size != 0) {
        _at.cmd_start_stop("+CFTPSPUT", "");
        err = read_fuzzy_ftp_response(_at, true, false, "+CFTPSPUT");
        RETURN_IF_ERROR(err);
    } else {
        // note: sim5320 implementation doesn't allow to create empty files, so skip them
        tr_info("Cannot create empty file. Skip it ...");
    }

    return data_writer_error >= 0 ? NSAPI_ERROR_OK : data_writer_error;
}

namespace sim5320 {
struct buffer_reader_t {
    uint8_t *src_buf;
    size_t src_len;
    size_t i;

    buffer_reader_t(uint8_t *src_buf, size_t src_len, size_t i)
        : src_buf(src_buf)
        , src_len(src_len)
        , i(i)
    {
    }

    ssize_t read(uint8_t *buf, size_t len)
    {
        size_t not_sent_len = src_len - i;
        size_t transfer_size = len < not_sent_len ? len : not_sent_len;
        memcpy(buf, src_buf + i, transfer_size);
        i += transfer_size;
        return (ssize_t)transfer_size;
    }
};
}

nsapi_error_t SIM5320FTPClient::put(const char *path, uint8_t *buf, size_t len)
{
    buffer_reader_t buffer_reader(buf, len, 0);
    return put(path, callback(&buffer_reader, &buffer_reader_t::read));
}

nsapi_error_t SIM5320FTPClient::get(const char *path, Callback<ssize_t(uint8_t *, size_t)> data_reader)
{
    return _get_data_impl(path, data_reader, "GET");
}

nsapi_error_t SIM5320FTPClient::download(const char *remote_path, const char *local_path)
{
    int err;
    int file;

    file = open(local_path, O_WRONLY | O_CREAT | O_TRUNC);
    if (file < 0) {
        return MBED_ERROR_EIO;
    }

    err = download(remote_path, file);

    if (close(file)) {
        err = any_error(err, MBED_ERROR_EIO);
    }

    return err;
}

namespace sim5320 {
struct cfile_download_callback_t {
    FILE *file;

    cfile_download_callback_t(FILE *file)
        : file(file)
    {
    }

    ssize_t store(uint8_t *buf, size_t len)
    {
        size_t write_res = fwrite(buf, sizeof(uint8_t), len, file);
        if (write_res != len) {
            return MBED_ERROR_EIO;
        } else {
            return len;
        }
    }
};
}

nsapi_error_t SIM5320FTPClient::download(const char *remote_path, FILE *local_file)
{
    cfile_download_callback_t donwload_callback(local_file);
    return get(remote_path, callback(&donwload_callback, &cfile_download_callback_t::store));
}

namespace sim5320 {
struct file_download_callback_t {
    int dst_file;

    file_download_callback_t(int dst_file)
        : dst_file(dst_file)
    {
    }

    ssize_t store(uint8_t *buf, size_t len)
    {
        ssize_t write_res = write(dst_file, buf, len);
        if (write_res != len) {
            return MBED_ERROR_EIO;
        } else {
            return len;
        }
    }
};
}

nsapi_error_t SIM5320FTPClient::download(const char *remote_path, int local_file)
{
    file_download_callback_t donwload_callback(local_file);
    return get(remote_path, callback(&donwload_callback, &file_download_callback_t::store));
}

nsapi_error_t SIM5320FTPClient::upload(const char *local_path, const char *remote_path)
{
    int err;
    int file;

    file = open(local_path, O_RDONLY);
    if (file < 0) {
        return MBED_ERROR_EIO;
    }

    err = upload(file, remote_path);

    if (close(file)) {
        err = any_error(err, MBED_ERROR_EIO);
    }

    return err;
}

namespace sim5320 {
struct cfile_upload_callback_t {
    FILE *file;

    cfile_upload_callback_t(FILE *file)
        : file(file)
    {
    }

    ssize_t fetch(uint8_t *buf, size_t len)
    {
        errno = 0;
        size_t read_res = fread(buf, sizeof(uint8_t), len, file);
        if (read_res == 0) {
            if (feof(file)) {
                return 0;
            } else {
                return MBED_ERROR_EIO;
            }
        } else {
            return read_res;
        }
    }
};
}

nsapi_error_t SIM5320FTPClient::upload(FILE *local_file, const char *remote_path)
{
    cfile_upload_callback_t upload_callback(local_file);
    return put(remote_path, callback(&upload_callback, &cfile_upload_callback_t::fetch));
}

namespace sim5320 {
struct file_upload_callback_t {
    int file;

    file_upload_callback_t(int file)
        : file(file)
    {
    }

    ssize_t fetch(uint8_t *buf, size_t len)
    {
        errno = 0;
        ssize_t read_res = read(file, buf, len);
        if (read_res < 0) {
            return MBED_ERROR_EIO;
        } else {
            return read_res;
        }
    }
};
}

nsapi_error_t SIM5320FTPClient::upload(int local_file, const char *remote_path)
{
    file_upload_callback_t upload_callback(local_file);
    return put(remote_path, callback(&upload_callback, &file_upload_callback_t::fetch));
}

static constexpr milliseconds_u32 FTP_GET_DATA_WAIT_TIMEOUT = 3s;
#define FTP_GET_DATA_MAX_WAIT_DATA_ATTEMPTS 10

nsapi_error_t SIM5320FTPClient::_get_data_impl(const char *path, Callback<ssize_t(uint8_t *, size_t)> data_reader, const char *command)
{
    ssize_t callback_res = 0;
    ATHandlerLocker locker(_at, FTP_RESPONSE_TIMEOUT);

    uint8_t *cache_buf = (uint8_t *)_get_buffer();

    // prepare commands
    const char *cmd_request;
    const char *cmd_response;
    bool add_rest_size;
    if (strcmp(command, "GET") == 0) {
        cmd_request = "AT+CFTPSGET=";
        cmd_response = "+CFTPSGET: ";
        add_rest_size = true;
    } else if (strcmp(command, "LIST") == 0) {
        cmd_request = "AT+CFTPSLIST=";
        cmd_response = "+CFTPSLIST: ";
        add_rest_size = false;
    } else {
        return NSAPI_ERROR_PARAMETER;
    }

    // request to get file using cache
    int cftpsget_code = -1;
    ssize_t data_len;
    _at.cmd_start(cmd_request);
    _at.write_string(path); // file path
    if (add_rest_size) {
        _at.write_int(0); // rest size
    }
    _at.write_int(1); // use cache
    _at.cmd_stop_read_resp();

    // read data from cache
    int wait_data_attempt_count = 0;
    while (!_at.get_last_error()) {
        _at.cmd_start_stop("+CFTPSCACHERD", "");
        // there 3 possible responses
        // 1. cache is empty:
        //      "OK"
        // 2. URC code that indicates end of transmission:
        //      "+CFTPS<CMD>: <code>"
        //    but we cannot differ it from "AT+CFTPSCACHERD" response
        // 3. cache isn't empty:
        //      "+CFTPS<CMD>: DATA, <len>"
        //      <content>
        //      OK
        _at.resp_start(cmd_response);
        bool cache_is_empty = true;
        while (_at.info_resp()) {
            //_at.set_stop_tag("\r\n");
            char cftpsget_param[5];
            _at.read_string(cftpsget_param, 5);
            tr_debug("Read: %s", cftpsget_param);
            if (strcmp(cftpsget_param, "DATA") == 0) {
                // it's data response
                data_len = _at.read_int();
                // process data
                _at.read_bytes(cache_buf, data_len);
                tr_debug("receive %d bytes", data_len);

                // process data by callback
                int processed_bytes = 0;
                while (processed_bytes < data_len) {
                    callback_res = data_reader(cache_buf + processed_bytes, data_len - processed_bytes);
                    if (callback_res < 0) {
                        break;
                    }
                    processed_bytes += callback_res;
                }
                if (callback_res < 0) {
                    tr_debug("callback returned %d. Stop data reading", callback_res);
                    break;
                }

                cache_is_empty = false;
            } else {
                // we get transmission code
                int code = atoi(cftpsget_param);
                tr_debug("GET URC code %d", code);
                cftpsget_code = code < 0 ? 2 : code;
            }
        }

        // as the operation can be long we should reset ATHanlder timeout
        // TODO: check if it's better to release lock and invoke ThisThread::yield to
        // allow other thread to use this driver.
        locker.reset_timeout();

        if (cache_is_empty) {
            if (cftpsget_code < 0) {
                // wait data
                wait_data_attempt_count++;
                if (wait_data_attempt_count >= FTP_GET_DATA_MAX_WAIT_DATA_ATTEMPTS) {
                    cftpsget_code = 2;
                    break;
                }
                tr_debug("wait data ...");
                ThisThread::sleep_for(FTP_GET_DATA_WAIT_TIMEOUT);
            } else {
                // end of transmission
                tr_debug("Complete");
                break;
            }
        } else {
            wait_data_attempt_count = 0;
        }
    }

    if (cftpsget_code > 0) {
        return convert_ftp_error_code(cftpsget_code);
    }

    if (callback_res < 0) {
        return callback_res;
    }

    return _at.get_last_error();
}
