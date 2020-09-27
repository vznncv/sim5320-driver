#ifndef SIM5320_CELLULARINFORMATION_H
#define SIM5320_CELLULARINFORMATION_H

#include "mbed.h"

#include "ATHandler.h"
#include "CellularInformation.h"

namespace sim5320 {

class SIM5320CellularInformation : public CellularInformation, private NonCopyable<SIM5320CellularInformation> {
protected:
    ATHandler &_at;

public:
    SIM5320CellularInformation(ATHandler &at_handler);
    virtual ~SIM5320CellularInformation();

public:
    // CellularInformation interface
    virtual nsapi_error_t get_manufacturer(char *buf, size_t buf_size) override;
    virtual nsapi_error_t get_model(char *buf, size_t buf_size) override;
    virtual nsapi_error_t get_revision(char *buf, size_t buf_size) override;
    virtual nsapi_error_t get_serial_number(char *buf, size_t buf_size, SerialNumberType type) override;
    virtual nsapi_error_t get_imsi(char *imsi, size_t buf_size) override;
    virtual nsapi_error_t get_iccid(char *buf, size_t buf_size) override;

private:
    /**
     * Request information text from cellular device
     */
    nsapi_error_t _get_simcom_info(const char *cmd, const char *cmd_chr, const char *cmd_data, const char *resp_prefix, char *resp_buf, size_t buf_size);
};
}
#endif // SIM5320_CELLULARINFORMATION_H
