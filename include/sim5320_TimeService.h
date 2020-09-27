#ifndef SIM5320_TIMESERVICE_H
#define SIM5320_TIMESERVICE_H

#include "mbed.h"

#include "ATHandler.h"

namespace sim5320 {

/**
 * Helper API to get current time using network.
 */
class SIM5320TimeService : private NonCopyable<SIM5320TimeService> {
protected:
    ATHandler &_at;

public:
    SIM5320TimeService(ATHandler &at);
    virtual ~SIM5320TimeService();

private:
    const char *const *_htp_servers;
    size_t _htp_servers_num;

    nsapi_error_t _sync_time_with_htp_servers(const char *const servers[], size_t size);

    nsapi_error_t _read_modem_clk(time_t *time);

public:
    // TODO: add NTP support

    /**
     * Enable/disable automatic time and time zone update.
     *
     * @param state
     * @return 0 on success, otherwise non-zero value
     */
    nsapi_error_t set_tzu(bool state);

    /**
     * Get automatic time and time zone update state.
     *
     * @param state
     * @return 0 on success, otherwise non-zero value
     */
    nsapi_error_t get_tzu(bool &state);

    /**
     * Set list of the web server that can be used to update time.
     *
     * The method doesn't create a copy of @p servers array so an external code shouldn't modify it after method invocation.
     *
     * @param servers array of servers. Server string has the following format: "<dns_name>[:<port>]"
     * @param size len of the servers
     * @return 0 on success, otherwise non-zero value
     */
    nsapi_error_t set_htp_servers(const char *const servers[], size_t size);

    /**
     * Try to synchronize time using different approaches.
     *
     * Note: to use synchronization, a network should be configured and opened.
     *
     * @return 0 on success, otherwise non-zero value
     */
    nsapi_error_t sync_time();

    /**
     * Get current UTC time.
     *
     * note: there is no guarantee that time is valid
     *
     * @param time
     * @return 0 on success, otherwise non-zero value
     */
    nsapi_error_t get_time(time_t *time);
};
}

#endif // SIM5320_TIMESERVICE_H
