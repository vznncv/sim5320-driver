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
    virtual nsapi_size_or_error_t socket_recvfrom_impl(CellularSocket* socket, SocketAddress* address, void* buffer, nsapi_size_t size);

private:
    void notify_socket(int socket_id);
    void disconnect_socket(int socket_id);
    // URC handlers
    void urc_cchevent();
    void urc_cch_peer_closed();
    void urc_cipevent();
    void urc_ipclose();
};
}

#endif // SIM5320_CELLULARSTACK_H
