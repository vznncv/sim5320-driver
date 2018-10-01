#ifndef SIM5320_CELLULARINFORMATION_H
#define SIM5320_CELLULARINFORMATION_H
#include "AT_CellularInformation.h"
#include "mbed.h"
namespace sim5320 {

class SIM5320CellularInformation : public AT_CellularInformation, private NonCopyable<SIM5320CellularInformation> {
public:
    SIM5320CellularInformation(ATHandler& atHandler);
    virtual ~SIM5320CellularInformation();

public:
    virtual nsapi_error_t get_manufacturer(char* buf, size_t buf_size);

    virtual nsapi_error_t get_model(char* buf, size_t buf_size);

    virtual nsapi_error_t get_revision(char* buf, size_t buf_size);

    virtual nsapi_error_t get_serial_number(char* buf, size_t buf_size, SerialNumberType type);

private:
    /** Request information text from cellular device
     *
     *  @param cmd 3gpp command string
     *  @param response_prefix prefix if response has it. If response have no prefix, then it should be NULL
     *  @param buf buffer for response
     *  @param buf_size buffer size
     *  @return on success 0, on failure negative error code
     */
    nsapi_error_t get_simcom_info(const char* cmd, const char* response_prefix, char* buf, size_t buf_size);
};
}
#endif // SIM5320_CELLULARINFORMATION_H
