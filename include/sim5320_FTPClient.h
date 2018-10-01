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
    /*
     * Helper function to read FTP command result code
     */
    nsapi_error_t _read_ftp_ret_code(const char* command_name, bool path_timeout = true);

public:
    // ftp API

    enum FTPProtocol {
        FTP = 0,
        FTPS_SSL = 1,
        FTPS_TLS = 2,
        FTPS = 3
    };

    enum FTPErrorCode {
        FTP_ERROR_SSL = -4001,
        FTP_ERROR_UNKNONW = -4002,
        FTP_ERROR_BUSY = -4003,
        FTP_ERROR_CLOSED_CONNECTION = -4004,
        FTP_ERROR_TIMEOUT = -4005,
        FTP_ERROR_TRANSFER_FAILED = -4006,
        FTP_ERROR_MEMORY = -4007,
        FTP_ERROR_INVALID_PARAMTER = -4008,
        FTP_ERROR_OPERATION_REJECTED_BY_SERVER = -4009,
        FTP_ERROR_NETWROK_ERROR = -4010
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
    nsapi_error_t get_cwd(char work_dir, size_t max_size);

    /**
    * Set current working directory.
    *
    * @param work_dir
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t set_cwd(const char* work_dir);

    /**
    * List files in the specified directory.
    *
    * For each filename the @p name_callback will be invoked with `filename`.
    * To get full filepath you should concatenate @p path and `filename`.
    *
    * @param path directory path
    * @param name_callback callback
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t list_dir(const char* path, Callback<void(const char*)> name_callback);

    /**
    * Check if file exists on a ftp server.
    *
    * @param path file path
    * @param result true if file exists, otherwise false
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
    * @param path ftp file path
    * @param data_reader callback to provide data
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t put(const char* path, Callback<ssize_t(uint8_t* data, ssize_t size)> data_reader);

    /**
    * Get file from ftp server.
    *
    * The read data will be processed by @p data_writer callback. It accepts buffer `data`, its length `size`,
    * and returns amount of the data that has been processed.
    *
    * @param path ftp file path
    * @param data_writer callback
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t get(const char* path, Callback<ssize_t(uint8_t* data, ssize_t size)> data_writer);

    /**
    * @brief ftp_close
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t close();
};
}

#endif // SIM5320_FTPCLIENT_H
