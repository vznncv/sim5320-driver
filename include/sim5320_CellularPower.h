#ifndef SIM5320_CELLULARPOWER_H
#define SIM5320_CELLULARPOWER_H

#include "AT_CellularPower.h"
#include "mbed.h"

namespace sim5320 {

class SIM5320CellularPower : public AT_CellularPower, private NonCopyable<SIM5320CellularPower> {
public:
    SIM5320CellularPower(ATHandler& atHandler);
    virtual ~SIM5320CellularPower();

private:
    virtual nsapi_error_t exec_cfun(int func_level);

public:
    virtual nsapi_error_t reset();
    virtual nsapi_error_t set_power_level(int func_level, int do_reset = 0);

private:
    /**
     * After module start it can show initialization messages like:
     * @code
     *     START
     *
     *     +STIN: 25
     *
     *     +STIN: 25
     *
     *     +CPIN: READY
     *
     *     SMS DONE
     *
     *     PB DONE
     * @endcode
     *
     * but ignore it.
     */
    void _skip_initialization_messages(int timeout);

public:
    virtual nsapi_error_t opt_power_save_mode();
    virtual nsapi_error_t opt_receive_period(int mode, EDRXAccessTechnology act_type, uint8_t edrx_value);
};
}

#endif // SIM5320_CELLULARPOWER_H
