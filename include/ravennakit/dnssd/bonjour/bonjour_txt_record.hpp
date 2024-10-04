#pragma once

#include "bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

#include "ravennakit/dnssd/service_description.hpp"

#include <map>
#include <string>

namespace rav::dnssd
{

/**
 * Class for holding and working with a TXTRecordRef
 */
class bonjour_txt_record
{
public:
    explicit bonjour_txt_record (const txt_record& txtRecord);
    ~bonjour_txt_record();

    /**
     * Sets a value inside the TXT record.
     * @param key Key.
     * @param value Value.
     * @return A result indicating success or failure.
     */
    void setValue(const std::string& key, const std::string& value);

    /**
     * Sets an empty value for key inside the TXT record.
     * @param key Key.
     * @return An Result indicating success or failure.
     */
    void setValue(const std::string& key);

    /**
     * @return Returns the length of the TXT record.
     */
    [[nodiscard]] uint16_t length() const noexcept;

    /**
     * @return Returns a pointer to the TXT record data. This pointer will be valid for as long as this instance lives.
     */
    [[nodiscard]] const void* bytesPtr() const noexcept;

    /**
     * Creates a TxtRecord from raw TXT record bytes.
     * @param txtRecord The txt record data.
     * @param txtRecordLength The length of the txt record.
     * @return The filled TxtRecord.
     */
    static txt_record get_txt_record_from_raw_bytes (const unsigned char* txtRecord, uint16_t txtRecordLength) noexcept;

private:
    TXTRecordRef txt_record_ref_{};
};

} // namespace dnssd

#endif
