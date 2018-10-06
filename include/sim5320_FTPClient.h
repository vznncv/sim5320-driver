#ifndef SIM5320_FTPCLIENT_H
#define SIM5320_FTPCLIENT_H

#include "AT_CellularBase.h"
#include "mbed.h"

namespace sim5320 {

class SIM5320FTPClient : public AT_CellularBase, private NonCopyable<SIM5320FTPClient> {
public:
    SIM5320FTPClient(ATHandler& at);
    virtual ~SIM5320FTPClient();

private:
    /**
     * Process ftp response like:
     *
     * @code
     * OK
     *
     * +FTPSCMD: <x>
     * @endcode
     *
     * @code
     * ERROR
     * @endcode
     *
     * @param command_name
     * @return
     */
    nsapi_error_t _process_ftp_ret_code(const char* command_name);

    /**
     * The same at ATHandler::lock but also clear any ftp errors and set ftp operation timeout.
     */
    void _lock();
    void _unlock();

    nsapi_error_t _last_ftp_error;
    int8_t _lock_count;

    nsapi_error_t _unlock_return_error();
    nsapi_error_t _get_last_error();
    nsapi_error_t _get_last_ftp_error();

    void _clear_error(bool at_error = true, bool ftp_error = true);
    void _set_ftp_error_code(int err_code);

    void _read_ftp_code();

    // helper buffer for different purposes
    nsapi_error_t _init_buffer();
    static const size_t _STR_BUF_SIZE = 256;
    char* _str_buf;

public:
    // ftp API

    enum FTPProtocol {
        FTP = 0,
        FTPS_SSL = 1,
        FTPS_TLS = 2,
        FTPS = 3
    };

    enum FTPErrorCode {
        FTP_ERROR_RESPONSE_CODE = 4000, // invalid response code
        FTP_ERROR_SSL = -4001,
        FTP_ERROR_UNKNONW = -4002,
        FTP_ERROR_BUSY = -4003,
        FTP_ERROR_CLOSED_CONNECTION = -4004,
        FTP_ERROR_TIMEOUT = -4005,
        FTP_ERROR_TRANSFER_FAILED = -4006,
        FTP_ERROR_MEMORY = -4007,
        FTP_ERROR_INVALID_PARAMTER = -4008,
        FTP_ERROR_OPERATION_REJECTED_BY_SERVER = -4009,
        FTP_ERROR_NETWROK_ERROR = -4010,
        FTP_ERROR_DRIVER_MEMORY = -4011 // error memory allocation for driver
    };

    /**
    * Connect to ftp server.
    *
    * @param host ftp server host
    * @param port ftp port
    * @param protocol ftp protocol (ftp or ftps)
    * @param username username. If it isn't set then "anonymous"
    * @param password user password
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t connect(const char* host, int port = 21, FTPProtocol protocol = FTP, const char* username = "anonymous", const char* password = "");

    /**
    * Get current working directory.
    *
    * @param work_dir
    * @param max_size size of the workdir buffer.
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t get_cwd(char* work_dir, size_t max_size);

    /**
    * Set current working directory.
    *
    * @param work_dir
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t set_cwd(const char* work_dir);

    // TODO: add method to list files in the directory

    /**
     * Get file size in bytes.
     *
     * @param size file size or negative value if file doesn't exists
     * @return
     */
    nsapi_error_t get_file_size(const char* path, long& size);

    /**
    * Check if file exists on a ftp server.
    *
    * @param path file path
    * @param result true if file exists, otherwise false
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t isfile(const char* path, bool& result);

    /**
     * Check if given path is directory.
     *
     * @param path directory path
     * @param result true if directory exists, otherwise false
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t isdir(const char* path, bool& result);

    /**
     * Check if file or directory exists.
     *
     * @param path file or directory path
     * @param result rue if path exists, otherwise false
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t exists(const char* path, bool& result);

    /**
    * Create a directory on a ftp server.
    *
    * @param path directory path
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t mkdir(const char* path);

    /**
    * Remove a directory on a ftp server.
    *
    * @param path directory path
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t rmdir(const char* path);

    /**
    * Remove a file on a ftp server.
    *
    * @param path file path
    * @return 0 on success, non-zero on failure
    */
    bool rmfile(const char* path);

    /**
    * Put file on an ftp server.
    *
    * The data source is callback @p data_reader. It accept buffer `data` and maximum data length `size`
    * that it can accept. The callback should return actual amount of data that it put into `data`.
    * If returned value is `0`, then transmission will be finished.
    *
    * @note
    * This operation can be long and lock ATHandler object, so you cannot use other sim5320 functionality
    * till end of this operation.
    *
    * @param path ftp file path
    * @param data_writer callback to provide data
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t put(const char* path, Callback<ssize_t(uint8_t* data, size_t size)> data_writer);

    /**
    * Get file from ftp server.
    *
    * The read data will be processed by @p data_writer callback. It accepts buffer `data`, its length `size`,
    * and returns amount of the data that has been processed.
    *
    * @param path ftp file path
    * @param data_reader callback
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t get(const char* path, Callback<ssize_t(uint8_t* data, size_t size)> data_reader);

    /**
    * @brief ftp_close
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t disconnect();
};
}

#endif // SIM5320_FTPCLIENT_H
