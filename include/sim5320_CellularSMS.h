/**
 * SIM5320 CellularSMS interface implementation.
 *
 * Partially this code is based on a mbed-os/connectivity/cellular/source/framework/AT/AT_CellularSMS.cpp file code,
 * so it's distributed under original license Apache-2.0 and holds original copyright:
 * - copyright (c) 2017, Arm Limited and affiliates.
 */
#ifndef SIM5320_CELLULARSIM_H
#define SIM5320_CELLULARSIM_H

#if MBED_CONF_CELLULAR_USE_SMS

#include "mbed.h"

#include "ATHandler.h"
#include "CellularSMS.h"

namespace sim5320 {

/**
 * CellularSMS interface implementation.
 *
 * To simplify implementation only SMS text mode is supported.
 */
class SIM5320CellularSMS : public CellularSMS, private NonCopyable<SIM5320CellularSMS> {
protected:
    ATHandler &_at;

    Callback<void()> _cb;
    bool _use_8bit_encoding = true;
    uint32_t _sim_wait_time;
    uint16_t _sms_message_ref_number;

    // URC callbacks
    void _cmti_urc();
    void _cmt_urc();

protected:
    nsapi_error_t get_sms_message_mode(CellularSMS::CellularSMSMmode &mode);

public:
    SIM5320CellularSMS(ATHandler &at_handler);
    virtual ~SIM5320CellularSMS();

public:
    // CellularSMS interface
    nsapi_error_t initialize(CellularSMSMmode mode, CellularSMSEncoding encoding);
    nsapi_size_or_error_t send_sms(const char *phone_number, const char *message, int msg_len);
    nsapi_size_or_error_t get_sms(char *buf, uint16_t buf_len, char *phone_num, uint16_t phone_len, char *time_stamp, uint16_t time_len, int *buf_size);
    void set_sms_callback(Callback<void()> func);
    nsapi_error_t set_cpms(const char *memr, const char *memw, const char *mems);
    nsapi_error_t set_csca(const char *sca, int type);
    nsapi_size_or_error_t set_cscs(const char *chr_set);
    nsapi_error_t delete_all_messages();
    void set_extra_sim_wait_time(int sim_wait_time);

    //    virtual nsapi_size_or_error_t send_sms(const char *phone_number, const char *message, int msg_len) override;

    //    /**
    //     * Get last sms message.
    //     *
    //     * Unlike standard implementation:
    //     *
    //     * 1. It doesn't use dynamic memory allocation.
    //     * 2. It doesn't have side-effect: delete read message
    //     * 3. Support only text messages.
    //     *
    //     * @param buf
    //     * @param buf_len
    //     * @param phone_num
    //     * @param phone_len
    //     * @param time_stamp
    //     * @param time_len
    //     * @param buf_size
    //     * @return
    //     */
    //    virtual nsapi_size_or_error_t get_sms(char *buf, uint16_t buf_len, char *phone_num, uint16_t phone_len, char *time_stamp, uint16_t time_len, int *buf_size) override;

    //protected:
    //    /**
    //     * Get current SMS mode.
    //     *
    //     * @param mode
    //     * @return
    //     */
    //    nsapi_error_t get_sms_message_mode(CellularSMSMmode &mode);
};
}

#endif // MBED_CONF_CELLULAR_USE_SMS

#endif // SIM5320_CELLULARSIM_H
