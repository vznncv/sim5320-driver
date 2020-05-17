#ifndef SIM5320_CELLULARINFORMATION_H
#define SIM5320_CELLULARINFORMATION_H
#include "AT_CellularInformation.h"
#include "mbed.h"

namespace sim5320 {

class SIM5320CellularInformation : public AT_CellularInformation, private NonCopyable<SIM5320CellularInformation> {
public:
    SIM5320CellularInformation(ATHandler &at_handler, AT_CellularDevice &device);
    virtual ~SIM5320CellularInformation();

public:
    virtual nsapi_error_t get_manufacturer(char *buf, size_t buf_size) override;

    virtual nsapi_error_t get_model(char *buf, size_t buf_size) override;

    virtual nsapi_error_t get_revision(char *buf, size_t buf_size) override;

    virtual nsapi_error_t get_serial_number(char *buf, size_t buf_size, SerialNumberType type) override;

    virtual nsapi_error_t get_imsi(char *imsi, size_t buf_size) override;

    virtual nsapi_error_t get_iccid(char *buf, size_t buf_size) override;

private:
    /** Request information text from cellular device
     *
     *  @param cmd 3gpp command string
     *  @param response_prefix prefix if response has it. If response have no prefix, then it should be NULL
     *  @param buf buffer for response
     *  @param buf_size buffer size
     *  @return 0 on success, non-zero on failure
     */
    nsapi_error_t _get_simcom_info(const char *cmd, const char *response_prefix, char *buf, size_t buf_size);
};
}
#endif // SIM5320_CELLULARINFORMATION_H
