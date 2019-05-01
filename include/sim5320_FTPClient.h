#ifndef SIM5320_FTPCLIENT_H
#define SIM5320_FTPCLIENT_H

#include "AT_CellularBase.h"
#include "CellularList.h"
#include "mbed.h"

namespace sim5320 {

/**
 * FTP client of the SIM5320
 */
class SIM5320FTPClient : public AT_CellularBase, private NonCopyable<SIM5320FTPClient> {
public:
    SIM5320FTPClient(ATHandler &at);
    virtual ~SIM5320FTPClient();

private:
    // helper buffer for different purposes
    char *_get_buffer();
    char *_buffer;
    bool _cleanup_buffer;

public:
    static const size_t BUFFER_SIZE = 1024;

    /**
     * Set buffer for an internal operations.
     *
     * Some internal operations requires a buffer that has size BUFFER_SIZE. If it isn't set, it will be allocated by requirement.
     * This method can be invoked only once and before any other actions.
     *
     * @brief set_buffer
     * @param buf
     * @return
     */
    nsapi_error_t set_buffer(uint8_t *buf);

    /**
     * FTP protocol type.
     */
    enum FTPProtocol {
        FTP = 0,
        FTPS_EXPLICIT_SSL = 1,
        FTPS_EXPLICIT_TLS = 2,
        FTPS_EXPLICIT = 2,
        FTPS_IMPLICIT = 3
    };

    /**
     * FTP error codes.
     */
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
    nsapi_error_t connect(const char *host, int port, FTPProtocol protocol, const char *username = "anonymous", const char *password = "");

    /**
     * Connect to ftp server
     *
     * Address has the following format:
     * @code
     * <protocol>://<username>:<password>@<hostname>:<port>
     * @endcode
     *
     * Protocol can be:
     * - "ftp" - for ftp
     * - "ftps" - for ftps with explicit encryption
     * - "ftps+e" - for ftps with explicit encryption
     * - "ftps+i" - for ftps with implicit encryption
     *
     * URL samples:
     *
     * - ftp://demo:password@test.rebex.net:21
     * - ftps://demo:password@test.rebex.net:21
     * - ftps+i://demo:password@test.rebex.net:990
     *
     * @brief connect
     * @param address
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t connect(const char *address);

    /**
     * @brief ftp_close
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t disconnect();

    /**
     * Get current working directory.
     *
     * @param work_dir
     * @param max_size size of the workdir buffer.
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t get_cwd(char *work_dir, size_t max_size);

    /**
     * Set current working directory.
     *
     * @param work_dir
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t set_cwd(const char *work_dir);

    /**
     * Get file size in bytes.
     *
     * @param size file size or negative value if file doesn't exists
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t get_file_size(const char *path, long &size);

    /**
    * Check if file exists on a ftp server.
    *
    * @param path file path
    * @param result true if file exists, otherwise false
    * @return 0 on success, non-zero on failure
    */
    nsapi_error_t isfile(const char *path, bool &result);

    /**
     * Check if given path is directory.
     *
     * @param path directory path
     * @param result true if directory exists, otherwise false
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t isdir(const char *path, bool &result);

    /**
     * Check if file or directory exists.
     *
     * @param path file or directory path
     * @param result rue if path exists, otherwise false
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t exists(const char *path, bool &result);

    /**
     * Create directory on a ftp server.
     *
     * @param path directory path
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t mkdir(const char *path);

    /**
     * Remove directory on a ftp server.
     *
     * @param path directory path
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t rmdir(const char *path);

private:
    nsapi_error_t _rmtree_impl(char *path_buf, size_t path_buf_len);

public:
    /**
     * Remove directory recursivy on a ftp server.
     *
     * warning: this method should be used for tests only as it can use dynamic memory allocation operations
     *
     * @param path directory path
     * @param remove_root if it's @c false, then remove directory content, but don't delete directory itself
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t rmtree(const char *path, bool remove_root = true);

    /**
     * Remove a file on a ftp server.
     *
     * @param path file path
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t rmfile(const char *path);

    struct dir_entry_t {
        char *name;
        /**
         * DT_REG, DT_DIR or DT_UNKNOWN.
         */
        char d_type;

        dir_entry_t *next;

    private:
        dir_entry_t();
        ~dir_entry_t();
        friend CellularList<dir_entry_t>;
    };

    /**
     * List of the entries that are returned by listdir method.
     */
    typedef CellularList<dir_entry_t> dir_entry_list_t;

    /**
     * Get list of the files in the specified directories.
     *
     * It isn't recommended to use this function for a production as it uses dynamic memory allocation operations.
     *
     * warning: the method cannot process correctly names that contain non-ascii symbols or spaces.
     *
     * @param path
     * @param dir_entry_list
     * @return
     */
    nsapi_error_t listdir(const char *path, dir_entry_list_t *dir_entry_list);

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
    nsapi_error_t put(const char *path, Callback<ssize_t(uint8_t *data, size_t size)> data_writer);

    /**
     * Put file on an ftp server.
     *
     * This version accept buffer instead of data reader.
     *
     * @param path ftp file path
     * @param buf buffer with a data
     * @param len buffer size
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t put(const char *path, uint8_t *buf, size_t len);

    /**
     * Get file from ftp server.
     *
     * The read data will be processed by @p data_writer callback. It accepts buffer `data`, its length `size`,
     * and returns amount of the data that has been processed. In case of error it should return negative value.
     *
     * @param path ftp file path
     * @param data_reader callback
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t get(const char *path, Callback<ssize_t(uint8_t *data, size_t size)> data_reader);

    /**
     * Download file from ftp server.
     *
     *
     * @param remote_path ftp file path
     *  @param local_path destination path
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t download(const char *remote_path, const char *local_path);

    /**
     * Download file from ftp server.
     *
     * @param remote_path ftp file path
     * @param local_file local file descriptor
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t download(const char *remote_path, FILE *local_file);

    /**
     * Upload file to ftp server.
     *
     * @param local_path local file location
     * @param remote_path ftp file path
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t upload(const char *local_path, const char *remote_path);

    /**
     * Upload file to ftp server.
     *
     * @param local_file local file descriptor
     * @param remote_path ftp file path
     * @return 0 on success, non-zero on failure
     */
    nsapi_error_t upload(FILE *local_file, const char *remote_path);

private:
    /**
     * Data reader implementation for get/listdir commands.
     *
     * @param path
     * @param data_reader
     * @param command
     * @return
     */
    nsapi_error_t _get_data_impl(const char *path, Callback<ssize_t(uint8_t *data, size_t size)> data_reader, const char *command);
};
}

#endif // SIM5320_FTPCLIENT_H
