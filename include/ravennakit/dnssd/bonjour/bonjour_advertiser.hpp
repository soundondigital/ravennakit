#pragma once

#include "bonjour.hpp"
#include "bonjour_shared_connection.hpp"
#include "process_results_thread.hpp"
#include "ravennakit/util/id.hpp"

#include <map>
#include <mutex>

#if RAV_HAS_APPLE_DNSSD

    #include "bonjour_scoped_dns_service_ref.hpp"
    #include "ravennakit/dnssd/dnssd_advertiser.hpp"
    #include "ravennakit/dnssd/service_description.hpp"

namespace rav::dnssd {

/**
 * Wrapper around dns_sd.h's DNSServiceRegister function.
 */
class bonjour_advertiser: public dnssd_advertiser {
  public:
    explicit bonjour_advertiser();
    ~bonjour_advertiser() override;

    util::id register_service(
        const std::string& reg_type, const char* name, const char* domain, uint16_t port, const txt_record& txt_record,
        bool auto_rename
    ) override;

    void update_txt_record(util::id id, const txt_record& txt_record) override;
    void unregister_service(util::id id) override;

  private:
    struct registered_service {
        util::id id;
        bonjour_scoped_dns_service_ref service_ref;
    };

    bonjour_shared_connection shared_connection_;
    process_results_thread process_results_thread_;
    util::id::generator id_generator_;
    std::vector<registered_service> registered_services_;

    static void DNSSD_API register_service_callback(
        DNSServiceRef service_ref, DNSServiceFlags flags, DNSServiceErrorType error_code, const char* service_name,
        const char* reg_type, const char* reply_domain, void* context
    );

    registered_service* find_registered_service(util::id id);
};

}  // namespace rav::dnssd

#endif
