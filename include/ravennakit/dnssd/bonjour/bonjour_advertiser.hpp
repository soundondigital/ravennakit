#pragma once

#include "bonjour.hpp"
#include "bonjour_shared_connection.hpp"
#include "bonjour_process_results_thread.hpp"
#include "ravennakit/core/util/id.hpp"

#include <map>
#include <mutex>
#include <boost/asio/ip/tcp.hpp>

#if RAV_HAS_APPLE_DNSSD

    #include "bonjour_scoped_dns_service_ref.hpp"
    #include "ravennakit/dnssd/dnssd_advertiser.hpp"
    #include "ravennakit/dnssd/dnssd_service_description.hpp"

namespace rav::dnssd {

/**
 * Wrapper around dns_sd.h's DNSServiceRegister function.
 */
class BonjourAdvertiser: public Advertiser {
  public:
    /**
     * Constructs a Bonjour advertiser.
     * Given io_context is used to process the results of the Bonjour service.
     * It is assumed that the io_context is run by a single thread.
     * @param io_context The context to use for the processing results.
     */
    explicit BonjourAdvertiser(boost::asio::io_context& io_context);

    Id register_service(
        const std::string& reg_type, const char* name, const char* domain, uint16_t port, const TxtRecord& txt_record,
        bool auto_rename, bool local_only
    ) override;

    void update_txt_record(Id id, const TxtRecord& txt_record) override;
    void unregister_service(Id id) override;

  private:
    struct registered_service {
        Id id;
        BonjourScopedDnsServiceRef service_ref;
    };

    boost::asio::ip::tcp::socket service_socket_;
    BonjourSharedConnection shared_connection_;
    Id::Generator id_generator_;
    std::vector<registered_service> registered_services_;
    size_t process_results_failed_attempts_ = 0;

    void async_process_results();

    static void DNSSD_API register_service_callback(
        DNSServiceRef service_ref, DNSServiceFlags flags, DNSServiceErrorType error_code, const char* service_name,
        const char* reg_type, const char* reply_domain, void* context
    );

    registered_service* find_registered_service(Id id);
};

}  // namespace rav::dnssd

#endif
