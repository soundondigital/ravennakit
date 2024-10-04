#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_txt_record.hpp"

    #include <dns_sd.h>
    #include <map>

rav::dnssd::bonjour_txt_record::bonjour_txt_record(const txt_record& txtRecord) {
    // By passing 0 and nullptr, TXTRecordCreate will arrange allocation for a buffer.
    TXTRecordCreate(&txt_record_ref_, 0, nullptr);

    for (auto& kv : txtRecord) setValue(kv.first, kv.second);
}

rav::dnssd::bonjour_txt_record::~bonjour_txt_record() {
    TXTRecordDeallocate(&txt_record_ref_);
}

void rav::dnssd::bonjour_txt_record::setValue(const std::string& key, const std::string& value) {
    DNSSD_THROW_IF_ERROR(
        TXTRecordSetValue(&txt_record_ref_, key.c_str(), static_cast<uint8_t>(value.length()), value.c_str()),
        "Failed to set txt record value"
    );
}

void rav::dnssd::bonjour_txt_record::setValue(const std::string& key) {
    DNSSD_THROW_IF_ERROR(TXTRecordSetValue(&txt_record_ref_, key.c_str(), 0, nullptr), "Failed to set txt record key");
}

uint16_t rav::dnssd::bonjour_txt_record::length() const noexcept {
    return TXTRecordGetLength(&txt_record_ref_);
}

const void* rav::dnssd::bonjour_txt_record::bytesPtr() const noexcept {
    return TXTRecordGetBytesPtr(&txt_record_ref_);
}

std::map<std::string, std::string> rav::dnssd::bonjour_txt_record::get_txt_record_from_raw_bytes(
    const unsigned char* txtRecordRawBytes, uint16_t txtRecordLength
) noexcept {
    std::map<std::string, std::string> txtRecord;

    const int keybuflen = 256;
    char key[keybuflen];
    uint8_t valuelen;
    const void* value;

    for (uint16_t i = 0; i < TXTRecordGetCount(txtRecordLength, txtRecordRawBytes); i++) {
        TXTRecordGetItemAtIndex(txtRecordLength, txtRecordRawBytes, i, keybuflen, key, &valuelen, &value);
        std::string strKey(key);
        std::string strValue(static_cast<const char*>(value), valuelen);
        txtRecord.insert({strKey, strValue});
    }

    return txtRecord;
}

#endif
