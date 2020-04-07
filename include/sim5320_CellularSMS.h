#ifndef SIM5320_CELLULARSIM_H
#define SIM5320_CELLULARSIM_H

#if MBED_CONF_CELLULAR_USE_SMS

#include "AT_CellularSMS.h"
#include "mbed.h"

namespace sim5320 {

class SIM5320CellularSMS : public AT_CellularSMS, private NonCopyable<SIM5320CellularSMS> {
public:
    SIM5320CellularSMS(ATHandler &at_handler);
    virtual ~SIM5320CellularSMS();

    virtual nsapi_size_or_error_t send_sms(const char *phone_number, const char *message, int msg_len);

    /**
     * Get last sms message.
     *
     * Unlike standard implementation:
     *
     * 1. It doesn't use dynamic memory allocation.
     * 2. It doesn't have side-effect: delete read message
     * 3. Support only text messages.
     *
     * @param buf
     * @param buf_len
     * @param phone_num
     * @param phone_len
     * @param time_stamp
     * @param time_len
     * @param buf_size
     * @return
     */
    virtual nsapi_size_or_error_t get_sms(char *buf, uint16_t buf_len, char *phone_num, uint16_t phone_len, char *time_stamp, uint16_t time_len, int *buf_size);

protected:
    /**
     * Get current SMS mode.
     *
     * @param mode
     * @return
     */
    nsapi_error_t get_sms_message_mode(CellularSMSMmode &mode);
};
}

#endif // MBED_CONF_CELLULAR_USE_SMS

#endif // SIM5320_CELLULARSIM_H
