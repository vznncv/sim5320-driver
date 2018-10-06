#ifndef SIM5320_CELLULARSIM_H
#define SIM5320_CELLULARSIM_H
#include "AT_CellularSMS.h"
#include "mbed.h"

namespace sim5320 {
class SIM5320CellularSMS : public AT_CellularSMS, private NonCopyable<SIM5320CellularSMS> {
public:
    SIM5320CellularSMS(ATHandler& at_handler);
    virtual ~SIM5320CellularSMS();

    virtual nsapi_size_or_error_t send_sms(const char* phone_number, const char* message, int msg_len);

    /**
     * Get last sms message.
     *
     * @note
     * It won't be deleted from memory.
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
    virtual nsapi_size_or_error_t get_sms(char* buf, uint16_t buf_len, char* phone_num, uint16_t phone_len,
        char* time_stamp, uint16_t time_len, int* buf_size);

protected:
    /**
     * Get current SMS mode.
     *
     * @param mode
     * @return
     */
    nsapi_error_t get_sms_message_mode(CellularSMSMmode& mode);
};
}
#endif // SIM5320_CELLULARSIM_H
