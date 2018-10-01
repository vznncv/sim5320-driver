#include "sim5320_CellularStack.h"
using namespace sim5320;

SIM5320CellularStack::SIM5320CellularStack(ATHandler& at, int cid, nsapi_ip_stack_t stack_type)
    : AT_CellularStack(at, cid, stack_type)
{
    _at.set_urc_handler("+CCHEVENT:", callback(this, &SIM5320CellularStack::urc_cchevent));
    _at.set_urc_handler("+CCH_PEER_CLOSED:", callback(this, &SIM5320CellularStack::urc_cch_peer_closed));
    _at.set_urc_handler("+CIPEVENT:", callback(this, &SIM5320CellularStack::urc_cipevent));
    _at.set_urc_handler("+IPCLOSE:", callback(this, &SIM5320CellularStack::urc_ipclose));
}

SIM5320CellularStack::~SIM5320CellularStack()
{
    _at.remove_urc_handler("+CCHEVENT:", callback(this, &SIM5320CellularStack::urc_cchevent));
    _at.remove_urc_handler("+CCH_PEER_CLOSED:", callback(this, &SIM5320CellularStack::urc_cch_peer_closed));
    _at.remove_urc_handler("+CIPEVENT:", callback(this, &SIM5320CellularStack::urc_cipevent));
    _at.remove_urc_handler("+IPCLOSE:", callback(this, &SIM5320CellularStack::urc_ipclose));
}

#define DNS_QUERY_TIMEOUT 30000

nsapi_error_t SIM5320CellularStack::gethostbyname(const char* host, SocketAddress* address, nsapi_version_t version)
{
    char ip_address[NSAPI_IP_SIZE];
    nsapi_error_t err = NSAPI_ERROR_NO_CONNECTION;

    _at.lock();
    if (address->set_ip_address(host)) {
        // the host is ip address, so skip
        err = NSAPI_ERROR_OK;
    } else {
        // This interrogation can sometimes take longer than the usual 8 seconds
        _at.cmd_start("AT+CDNSGIP=");
        _at.write_string(host);
        _at.cmd_stop();

        _at.set_at_timeout(DNS_QUERY_TIMEOUT);
        _at.resp_start("+CDNSGIP:");
        int ret_code = _at.read_int();
        if (ret_code == 1) {
            // skip name
            _at.skip_param();
            // read address
            _at.read_string(ip_address, NSAPI_IP_SIZE);
            if (address->set_ip_address(ip_address)) {
                err = NSAPI_ERROR_OK;
            }
        }
        _at.resp_stop();
        _at.restore_at_timeout();
    }
    _at.unlock();

    return err;
}

int SIM5320CellularStack::get_max_socket_count()
{
    return 10;
}

bool SIM5320CellularStack::is_protocol_supported(nsapi_protocol_t protocol)
{
    switch (protocol) {
    case NSAPI_TCP:
        return true;
    default:
        // TODO: add UDP support
        return false;
    }
}

#define TCP_OPEN_TIMEOUT 30000

nsapi_error_t SIM5320CellularStack::create_socket_impl(AT_CellularStack::CellularSocket* socket)
{
    int open_code = -1;
    // use socket index as socket id
    int socket_id = -1;
    for (int i = 0; i < get_max_socket_count(); i++) {
        if (_socket[i] == socket) {
            socket_id = i;
            break;
        }
    }
    if (socket_id < 0) {
        return NSAPI_ERROR_NO_SOCKET;
    }
    // ignore socket creation, if remote address isn't set
    if (!socket->remoteAddress) {
        return NSAPI_ERROR_NO_SOCKET;
    }

    socket->id = socket_id;
    _at.lock();
    if (socket->proto == NSAPI_TCP) {
        _at.cmd_start("AT+CIPOPEN=");
        _at.write_int(socket_id);
        _at.write_string("TCP");
        _at.write_string(socket->remoteAddress.get_ip_address());
        _at.write_int(socket->remoteAddress.get_port());
        _at.write_int(socket->localAddress.get_port());
        _at.cmd_stop();
        _at.resp_start();
        _at.resp_stop();
        // wait connection confirmation
        _at.set_at_timeout(TCP_OPEN_TIMEOUT);
        _at.resp_start("+CIPOPEN:", true);
        _at.skip_param();
        open_code = _at.read_int();
        _at.restore_at_timeout();
    } // unsupported protocol is checked in "is_protocol_supported" function
    nsapi_error_t err = _at.unlock_return_error();

    printf("socket creation error code %d\n", err);
    printf("socket creation open code %d\n", open_code);
    if (err || open_code != 0) {
        return NSAPI_ERROR_NO_SOCKET;
    }

    printf("finish socket creation\n");

    socket->created = true;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularStack::socket_close_impl(int sock_id)
{
    _at.lock();
    _at.cmd_start("AT+CIPCLOSE=");
    _at.write_int(sock_id);
    _at.cmd_stop();
    _at.resp_start();
    _at.resp_stop();
    return _at.unlock_return_error();
}

nsapi_size_or_error_t SIM5320CellularStack::socket_sendto_impl(AT_CellularStack::CellularSocket* socket, const SocketAddress& address, const void* data, nsapi_size_t size)
{
    printf("inside socket_sendto_impl\n");
    if (size == 0) {
        return 0;
    }
    if (socket->proto == NSAPI_TCP) {
        _at.lock();
        _at.cmd_start("AT+CIPSEND=");
        _at.write_int(socket->id);
        _at.write_int(size);
        _at.cmd_stop();
        printf("write data\n");
        wait_ms(5000);
        _at.write_bytes((const uint8_t*)data, size);
        _at.resp_start();

        _at.resp_start("+CIPSEND:", true);
        int link_num = _at.read_int();
        int req_send_length = _at.read_int();
        int cnf_send_length = _at.read_int();
        nsapi_error_t err = _at.get_last_error();
        _at.unlock();
        if (err) {
            return err;
        }
        if (cnf_send_length <= 0 || req_send_length != cnf_send_length) {
            // error, close socket
            socket->connected = false;
            socket->_cb(socket->_data);
            return NSAPI_ERROR_CONNECTION_LOST;
        }
        return req_send_length;
    } else {
        return NSAPI_ERROR_UNSUPPORTED;
    }
}

nsapi_size_or_error_t SIM5320CellularStack::socket_recvfrom_impl(AT_CellularStack::CellularSocket* socket, SocketAddress* address, void* buffer, nsapi_size_t size)
{
    printf("== inside socket_recvfrom_impl\n");
    nsapi_error_t err;
    if (size == 0) {
        return 0;
    }
    // limit size (it's probably should be limited by serial buffer size)
    if (size > 1500) {
        size = 1500;
    }
    if (socket->proto == NSAPI_TCP) {

        _at.lock();
        _at.cmd_start("+CIPRXGET=");
        _at.write_int(2); // read mode
        _at.write_int(socket->id); // socket id
        _at.write_int(size); // block to read
        _at.cmd_stop();

        _at.resp_start();
        _at.resp_stop();

        _at.resp_start("+CIPRXGET:");
        int mode = _at.read_int();
        int socket_id = _at.read_int();
        int read_len = _at.read_int();
        int rest_len = _at.read_int();
        // read NEWLINE
        uint8_t crnf_buff[2];
        _at.read_bytes(crnf_buff, 2);
        // read data
        _at.read_bytes((uint8_t*)buffer, size);
        _at.cmd_stop();
        err = _at.unlock_return_error();

        if (err) {
            return err;
        }
        return read_len;
    } else {
        return NSAPI_ERROR_UNSUPPORTED;
    }
}

void SIM5320CellularStack::notify_socket(int socket_id)
{
    if (socket_id >= 0 && socket_id < get_max_socket_count()) {
        CellularSocket* socket = _socket[socket_id];
        if (socket && socket->_cb) {
            socket->_cb(socket->_data);
        }
    }
}

void SIM5320CellularStack::disconnect_socket(int socket_id)
{
    if (socket_id >= 0 && socket_id < get_max_socket_count()) {
        CellularSocket* socket = _socket[socket_id];
        if (socket) {
            socket->connected = false;
            if (socket->_cb) {
                socket->_cb(socket->_data);
            }
        }
    }
}

void SIM5320CellularStack::urc_cchevent()
{
    int socket_id = _at.read_int();
    // new data is received
    notify_socket(socket_id);
}

void SIM5320CellularStack::urc_cch_peer_closed()
{
    int socket_id = _at.read_int();
    // socket is closed by peer
    disconnect_socket(socket_id);
}

void SIM5320CellularStack::urc_cipevent()
{
    // close all connection
    // TODO: find what should be done
    for (int i = 0; i < get_max_socket_count(); i++) {
        disconnect_socket(i);
    }
}

void SIM5320CellularStack::urc_ipclose()
{
    int socket_id = _at.read_int();
    // socket is closed by peer
    disconnect_socket(socket_id);
}
