#ifndef SIM5320_CELLULARSTACK_H
#define SIM5320_CELLULARSTACK_H

#include "AT_CellularStack.h"
#include "mbed.h"

namespace sim5320 {
class SIM5320CellularStack : public AT_CellularStack, private NonCopyable<SIM5320CellularStack> {
public:
    SIM5320CellularStack(ATHandler& at, int cid, nsapi_ip_stack_t stack_type);
    virtual ~SIM5320CellularStack();

    virtual nsapi_error_t gethostbyname(const char* host, SocketAddress* address, nsapi_version_t version = NSAPI_UNSPEC);

protected:
    virtual int get_max_socket_count();
    virtual bool is_protocol_supported(nsapi_protocol_t protocol);

    virtual nsapi_error_t create_socket_impl(CellularSocket* socket);
    virtual nsapi_error_t socket_close_impl(int sock_id);

    virtual nsapi_size_or_error_t socket_sendto_impl(CellularSocket* socket, const SocketAddress& address, const void* data, nsapi_size_t size);
    /**
     * Receive data.
     *
     * Return value:
     * - if data has been read, then number of the read bytes
     * - 0, if socket is closed
     * - NSAPI_ERROR_WOULD_BLOCK, if socket isn't close, but data isn't available
     * - other negative value in case of error.
     */
    virtual nsapi_size_or_error_t socket_recvfrom_impl(CellularSocket* socket, SocketAddress* address, void* buffer, nsapi_size_t size);

private:
    // map with active sockets
    uint16_t _active_sockets;
    // error of the AT+CIPRXGET
    bool _ciprxget_no_data;

    CellularSocket* _get_socket(int link_id);
    void _notify_socket(int link_id);
    void _notify_socket(CellularSocket* socket);
    void _disconnect_socket(int link_id);
    void _disconnect_socket(CellularSocket* socket);
    // URC handlers
    /**
     * The URC handler of the message:
     *
     * @code
     *  +CIPEVENT: NETWORK CLOSED
     * @endcode
     *
     * that indicates that the network has been closed and user application should close all sockets.
     */
    void _urc_cipevent();
    /**
     * The URC handler of the message:
     *
     * @code
     * +IPCLOSE: <link_id>,<close_reason>
     * @endcode
     *
     * that indicates socket has been closed.
     */
    void _urc_ipclose();
    /**
     * The URC handler of the message:
     *
     * @note
     * +RECEIVE,<link_id>,<data length>
     * @endcode
     *
     * that indicates that some socket has received data.
     */
    void _urc_receive();

    /**
     * The URC handler of the message:
     *
     * @note
     * +IP ERROR: No data
     * @endcode
     */
    void _urc_ciprxget_no_data();
};
}

#endif // SIM5320_CELLULARSTACK_H
