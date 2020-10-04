#include "sim5320_CellularStack.h"

#include "sim5320_trace.h"
#include "sim5320_utils.h"

using namespace sim5320;

SIM5320CellularStack::SIM5320CellularStack(ATHandler &at, int cid, nsapi_ip_stack_t stack_type, AT_CellularDevice &device)
    : AT_CellularStack(at, cid, stack_type, device)
    , _active_sockets(0)
{
    _at.set_urc_handler("+CIPEVENT:", callback(this, &SIM5320CellularStack::_urc_cipevent));
    _at.set_urc_handler("+IPCLOSE:", callback(this, &SIM5320CellularStack::_urc_ipclose));
    _at.set_urc_handler("+RECEIVE,", callback(this, &SIM5320CellularStack::_urc_receive));
    _at.set_urc_handler("+IP ERROR: No data", callback(this, &SIM5320CellularStack::_urc_ciprxget_no_data));
}

SIM5320CellularStack::~SIM5320CellularStack()
{
    _at.set_urc_handler("+CIPEVENT:", NULL);
    _at.set_urc_handler("+IPCLOSE:", NULL);
    _at.set_urc_handler("+RECEIVE,", NULL);
    _at.set_urc_handler("+IP ERROR: No data", NULL);
}

#define DNS_QUERY_TIMEOUT 32000

nsapi_error_t SIM5320CellularStack::gethostbyname(const char *host, SocketAddress *address, nsapi_version_t version, const char *interface_name)
{
    char ip_address[NSAPI_IP_SIZE];
    nsapi_error_t err = NSAPI_ERROR_NO_CONNECTION;

    if (address->set_ip_address(host)) {
        // the host is ip address, so skip
        err = NSAPI_ERROR_OK;
    } else {
        ATHandlerLocker locker(_at);
        _at.cmd_start("AT+CDNSGIP=");
        _at.write_string(host);
        _at.cmd_stop();

        _at.set_at_timeout(DNS_QUERY_TIMEOUT);
        _at.resp_start("+CDNSGIP:");
        int ret_code = _at.read_int();
        if (ret_code == 1) {
            // skip PDP context id
            _at.skip_param();
            // read address
            _at.read_string(ip_address, NSAPI_IP_SIZE);
            if (address->set_ip_address(ip_address)) {
                err = NSAPI_ERROR_OK;
            }
        } else {
            err = NSAPI_ERROR_DNS_FAILURE;
        }
        _at.resp_stop();
        _at.restore_at_timeout();

        err = any_error(err, _at.get_last_error());
    }

    return err;
}

#define TCP_OPEN_TIMEOUT 30000

nsapi_error_t SIM5320CellularStack::create_socket_impl(AT_CellularStack::CellularSocket *socket)
{
    int open_code = -1;
    int link_num;

    // use socket index as socket id
    int sock_id = -1;
    for (int i = 0; i < _device.get_property(AT_CellularDevice::PROPERTY_SOCKET_COUNT); i++) {
        if (_socket[i] == socket) {
            sock_id = i;
            break;
        }
    }
    if (sock_id < 0) {
        tr_debug("socket.create: cannot resolve socket id");
        return NSAPI_ERROR_NO_SOCKET;
    }

    socket->id = sock_id;
    ATHandlerLocker locker(_at);
    tr_debug("socket.create, sock_id %d: create ...", sock_id);
    if (socket->proto == NSAPI_TCP) {
        // ignore socket creation, if remote address isn't set
        if (!socket->remoteAddress) {
            tr_debug("socket.create, sock_id %d: remote address isn't set", sock_id);
            return NSAPI_ERROR_NO_SOCKET;
        }

        _at.cmd_start("AT+CIPOPEN=");
        _at.write_int(sock_id);
        _at.write_string("TCP");
        _at.write_string(socket->remoteAddress.get_ip_address());
        _at.write_int(socket->remoteAddress.get_port());
        _at.write_int(socket->localAddress.get_port());
        _at.cmd_stop();
        _at.resp_start();
        _at.resp_stop();
        // wait connection confirmation
        _at.set_at_timeout(TCP_OPEN_TIMEOUT);
        _at.resp_start("+CIPOPEN:");
        _at.restore_at_timeout();
        link_num = _at.read_int();
        open_code = _at.read_int();
        _at.consume_to_stop_tag();
    } else if (socket->proto == NSAPI_UDP) {
        _at.cmd_start("AT+CIPOPEN=");
        _at.write_int(sock_id);
        _at.write_string("UDP");
        if (socket->remoteAddress) {
            _at.write_string(socket->remoteAddress.get_ip_address());
            _at.write_int(socket->remoteAddress.get_port());
        } else {
            _at.write_string("", false);
            _at.write_string("", false);
        }
        _at.write_int(socket->localAddress.get_port());
        _at.cmd_stop();
        // check open result
        _at.resp_start("+CIPOPEN:");
        link_num = _at.read_int();
        open_code = _at.read_int();
        _at.consume_to_stop_tag();
    } else {
        return NSAPI_ERROR_UNSUPPORTED;
    }
    nsapi_error_t err = _at.get_last_error();
    if (link_num != sock_id) {
        tr_error("socket.create, sock_id %d: link number %d differs from socket id %d", sock_id, link_num, sock_id);
    }

    if (err || open_code != 0) {
        tr_debug("socket.create, sock_id %d: fail to create, err = %d, open_code = %d", sock_id, err, open_code);
        return NSAPI_ERROR_NO_SOCKET;
    }
    tr_debug("socket.create, sock_id %d: created", sock_id);

    socket->started = true;
    socket->pending_bytes = 0;
    _active_sockets |= 0x0001 << sock_id;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SIM5320CellularStack::socket_close_impl(int sock_id)
{
    ATHandlerLocker locker(_at);
    _at.cmd_start("AT+CIPCLOSE=");
    _at.write_int(sock_id);
    _at.cmd_stop();
    // get OK or ERROR (note: a URC code can appear before OK or ERROR)
    // FIXME: without delay it can cause freezing if a UDP socket is used
    ThisThread::sleep_for(10ms);
    _at.resp_start("OK");
    _at.resp_stop();

    if (!(_active_sockets & (0x0001 << sock_id))) {
        // ignore error if we tried to close the closed socket
        _at.clear_error();
    }
    // mark socket as closed
    _active_sockets &= ~(0x0001 << sock_id);
    tr_debug("socket.close, sock_id %d: closed (err %d)", sock_id, _at.get_last_error());

    return _at.get_last_error();
}

#define MAX_WRITE_BLOCK_SIZE 1500

nsapi_size_or_error_t SIM5320CellularStack::socket_sendto_impl(AT_CellularStack::CellularSocket *socket, const SocketAddress &address, const void *data, nsapi_size_t size)
{
    int sock_id = socket->id;
    tr_debug("socket.send, sock_id %d: send data ...", sock_id);
    if (size == 0) {
        tr_debug("socket.send, sock_id %d: no data to send", sock_id);
        return 0;
    }
    // if socket is closed, then return error
    if (!(_active_sockets & 0x0001 << sock_id)) {
        tr_debug("socket.send, sock_id %d: socket has been closed", sock_id);
        return NSAPI_ERROR_CONNECTION_LOST;
    }

    switch (socket->proto) {
    case NSAPI_TCP:
        if (size > MAX_WRITE_BLOCK_SIZE) {
            size = MAX_WRITE_BLOCK_SIZE;
        }
        break;
    case NSAPI_UDP:
        if (size > MAX_WRITE_BLOCK_SIZE) {
            return NSAPI_ERROR_PARAMETER;
        }
        break;
    default:
        return NSAPI_ERROR_UNSUPPORTED;
    }

    ATHandlerLocker locker(_at);
    // write send command
    switch (socket->proto) {
    case NSAPI_TCP:
        _at.cmd_start("AT+CIPSEND=");
        _at.write_int(socket->id);
        _at.write_int(size);
        _at.cmd_stop();
        break;
    case NSAPI_UDP:
        _at.cmd_start("AT+CIPSEND=");
        _at.write_int(socket->id);
        _at.write_int(size);
        _at.write_string(address.get_ip_address());
        _at.write_int(address.get_port());
        _at.cmd_stop();
        break;
    default:
        return NSAPI_ERROR_UNSUPPORTED;
    }
    // write data
    _at.resp_start(">", true);
    _at.write_bytes((const uint8_t *)data, size);

    // read actual amount of the data that has been send
    _at.resp_start();
    _at.resp_stop();
    _at.resp_start("+CIPSEND:");
    int link_id = _at.read_int();
    int req_send_length = _at.read_int();
    int cnf_send_length = _at.read_int();
    _at.consume_to_stop_tag();
    nsapi_error_t err = _at.get_last_error();

    if (link_id != sock_id) {
        tr_error("socket.send, sock_id %d: socket id %d differs from link id %d", sock_id, sock_id, link_id);
    }

    if (err) {
        tr_debug("socket.send, sock_id %d: fail to parse response", sock_id);
        return err;
    }
    if (cnf_send_length < 0 || req_send_length != cnf_send_length) {
        // error, close socket
        _active_sockets &= ~(0x0001 << sock_id);
        tr_debug("socket.send, sock_id %d: error, close socket", sock_id);
        return NSAPI_ERROR_CONNECTION_LOST;
    }

    if (req_send_length == 0) {
        tr_debug("socket.send, sock_id %d: socket is blocked", sock_id);
        return NSAPI_ERROR_WOULD_BLOCK;
    } else {
        tr_debug("socket.send, sock_id %d: %i bytes have been sent", sock_id, req_send_length);
        return req_send_length;
    }
}

#define MAX_READ_BLOCK_SIZE 230

nsapi_size_or_error_t SIM5320CellularStack::socket_recvfrom_impl(AT_CellularStack::CellularSocket *socket, SocketAddress *address, void *buffer, nsapi_size_t size)
{
    nsapi_error_t err;
    nsapi_size_or_error_t result;
    int mode;
    int link_id = -1;
    int read_len = 0;
    int rest_len = 0;

    int sock_id = socket->id;
    tr_debug("socket.recv, sock_id %d: receive data ...", sock_id);

    if (size == 0) {
        tr_debug("socket.recv, sock_id %d: nothing to send", sock_id);
        return 0;
    }

    // limit size (it's probably should be limited by serial buffer size)
    if (size > MAX_READ_BLOCK_SIZE) {
        size = MAX_READ_BLOCK_SIZE;
    }
    _at.process_oob();

    if (socket->pending_bytes == 0) {
        if (!(_active_sockets & 0x0001 << sock_id)) {
            // socket is closed and there are nothing to read
            tr_debug("socket.recv, sock_id %d: socket has been closed", sock_id);
            return 0;
        } else {
            tr_debug("socket.recv, sock_id %d: no data to read", sock_id);
            return NSAPI_ERROR_WOULD_BLOCK;
        }
    }

    switch (socket->proto) {
    case NSAPI_TCP:
    case NSAPI_UDP: {
        ATHandlerLocker locker(_at);
        // read data to free input buffer for data
        _at.cmd_start("AT+CIPRXGET=");
        _at.write_int(2); // read mode
        _at.write_int(socket->id); // socket id
        _at.write_int(size); // block to read
        _at.cmd_stop();

        _ciprxget_no_data = false;
        while (true) {
            _at.resp_start("+CIPRXGET:");
            mode = _at.read_int();
            if (mode < 2) {
                // note: we can get messages like
                // +CIPRXGET: 1,<link_id>, but we should ignore them
                _at.consume_to_stop_tag();
            } else {
                // we got message like this:
                // +CIPRXGET: <mode_2_or_3>,<link_id>,<read_len>,<rest_len>
                link_id = _at.read_int();
                read_len = _at.read_int();
                rest_len = _at.read_int();
                if (link_id != sock_id) {
                    tr_error("socket.recv, sock_id %d: socket id %d differs from link id %d", sock_id, sock_id, link_id);
                }
                break;
            }
            if (_at.get_last_error()) {
                break;
            }
        }

        _at.read_bytes((uint8_t *)buffer, read_len);
        _at.resp_stop();
        if (_ciprxget_no_data) {
            _at.clear_error();
        }
        err = _at.get_last_error();
        if (!err) {
            socket->pending_bytes -= read_len;
        }

        if (err) {
            tr_debug("socket.recv, sock_id %d: fail CIPRXGET command response", err);
            return err;
        }

        if (read_len == 0 || _ciprxget_no_data) {
            tr_debug("socket.recv, sock_id %d: no data to read", sock_id);
            result = NSAPI_ERROR_WOULD_BLOCK;
        } else {
            tr_debug("socket.recv, sock_id %d: %d bytes has been read", sock_id, read_len);
            result = read_len;
        }
        break;
    }
    default:
        result = NSAPI_ERROR_UNSUPPORTED;
    }

    return result;
}

AT_CellularStack::CellularSocket *SIM5320CellularStack::_get_socket(int link_id)
{
    if (link_id >= 0 && link_id < _device.get_property(AT_CellularDevice::PROPERTY_SOCKET_COUNT)) {
        CellularSocket *socket = _socket[link_id];
        return socket;
    }
    return NULL;
}

void SIM5320CellularStack::_notify_socket(int link_id)
{
    _notify_socket(_get_socket(link_id));
}

void SIM5320CellularStack::_notify_socket(AT_CellularStack::CellularSocket *socket)
{
    if (socket && socket->_cb) {
        socket->_cb(socket->_data);
    }
}

void SIM5320CellularStack::_disconnect_socket_by_peer(int link_id)
{
    _disconnect_socket_by_peer(_get_socket(link_id));
}

void SIM5320CellularStack::_disconnect_socket_by_peer(AT_CellularStack::CellularSocket *socket)
{
    if (!socket) {
        return;
    }

    // mark socket as closed
    _active_sockets &= ~(0x0001 << socket->id);

    if (socket->_cb) {
        socket->_cb(socket->_data);
    }
}

void SIM5320CellularStack::_urc_cipevent()
{
    _at.consume_to_stop_tag();
    // close all connection
    // TODO: find what should be done
    for (int i = 0; i < _device.get_property(AT_CellularDevice::PROPERTY_SOCKET_COUNT); i++) {
        _disconnect_socket_by_peer(i);
    }
}

void SIM5320CellularStack::_urc_ipclose()
{
    int link_id = _at.read_int();
    // socket is closed by peer
    _disconnect_socket_by_peer(link_id);
}

void SIM5320CellularStack::_urc_receive()
{
    int link_id = _at.read_int();
    int num_bytes = _at.read_int();

    CellularSocket *socket = _get_socket(link_id);
    if (!socket) {
        return;
    }
    // count pending bytes
    socket->pending_bytes += num_bytes;
    // notify socket
    _notify_socket(socket);
}

void SIM5320CellularStack::_urc_ciprxget_no_data()
{
    _ciprxget_no_data = true;
}
