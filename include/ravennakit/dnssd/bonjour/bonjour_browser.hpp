#pragma once

#include "bonjour.hpp"

#include <asio.hpp>

#if RAV_HAS_APPLE_DNSSD

#include "bonjour_scoped_dns_service_ref.hpp"
#include "bonjour_shared_connection.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace rav::dnssd {

/**
 * Apple Bonjour implementation of IBrowser. Works on macOS and Windows.
 */
class bonjour_browser: public dnssd_browser {
  public:
    /**
     * Represents a Bonjour service and holds state and methods for discovering and resolving services on the network.
     */
    class service {
      public:
        /**
         * Constructs a service.
         * @param fullname Full domain name of the service.
         * @param name Name of the service.
         * @param type Type of the service (i.e. _http._tcp)
         * @param domain The domain of the service (i.e. local.).
         * @param owner A reference to the owning BonjourBrowser.
         */
        service(
            const char* fullname, const char* name, const char* type, const char* domain, bonjour_browser& owner
        );

        /**
         * Called when a service was resolved.
         * @param index The index of the interface the service was resolved on.
         */
        void resolve_on_interface(uint32_t index);

        /**
         * Called when an interface was removed for this service.
         * @param index The index of the removed interface.
         * @return The amount of interfaces after the removal.
         */
        size_t remove_interface(uint32_t index);

        /**
         * @return Returns the ServiceDescription.
         */
        [[nodiscard]] const service_description& description() const noexcept;

        /**
         * Called by dns_sd callbacks when a service was resolved.
         * @param service_ref The DNSServiceRef.
         * @param flags Possible values: kDNSServiceFlagsMoreComing.
         * @param interface_index The interface on which the service was resolved.
         * @param error_code Will be kDNSServiceErr_NoError (0) on success, otherwise will indicate the failure that
         * occurred. Other parameters are undefined if the errorCode is nonzero.
         * @param fullname The full service domain name, in the form <servicename>.<protocol>.<domain>. (This name is
         *                  escaped following standard DNS rules, making it suitable for passing to standard system DNS
         * APIs such as res_query(), or to the special-purpose functions included in this API that take fullname
         *                  parameters. See "Notes on DNS Name Escaping" earlier in this file for more details.)
         * @param host_target The target hostname of the machine providing the service. This name can be passed to
         * functions like gethostbyname() to identify the host's IP address.
         * @param port The port, in network byte order, on which connections are accepted for this service.
         * @param txt_len The length of the txt record, in bytes.
         * @param txt_record The service's primary txt record, in standard txt record format.
         */
        void resolve_callback(
            DNSServiceRef service_ref, DNSServiceFlags flags, uint32_t interface_index, DNSServiceErrorType error_code,
            const char* fullname, const char* host_target,
            uint16_t port,  // In network byte order.
            uint16_t txt_len, const unsigned char* txt_record
        );

        /**
         * Called by dns_sd callbacks when an address was resolved.
         * @param sd_ref The DNSServiceRef
         * @param flags Possible values are kDNSServiceFlagsMoreComing and kDNSServiceFlagsAdd.
         * @param interface_index The interface to which the answers pertain.
         * @param error_code Will be kDNSServiceErr_NoError on success, otherwise will indicate the failure that
         * occurred. Other parameters are undefined if errorCode is nonzero.
         * @param hostname The fully qualified domain name of the host to be queried for.
         * @param address IPv4 or IPv6 address.
         * @param ttl Time to live.
         */
        void get_addr_info_callback(
            DNSServiceRef sd_ref, DNSServiceFlags flags, uint32_t interface_index, DNSServiceErrorType error_code,
            const char* hostname, const struct sockaddr* address, uint32_t ttl
        );

      private:
        bonjour_browser& owner_;
        std::map<uint32_t, bonjour_scoped_dns_service_ref> resolvers_;
        std::map<uint32_t, bonjour_scoped_dns_service_ref> get_addrs_;
        service_description description_;
    };

    explicit bonjour_browser(asio::io_context& io_context);
    ~bonjour_browser() override;

    void browse_for(const std::string& service) override;

    /**
     * @return Returns the SharedConnection this instance is using for communicating with the mdns responder.
     */
    [[nodiscard]] const bonjour_shared_connection& connection() const noexcept {
        return shared_connection_;
    }

  private:
    std::unique_ptr<asio::posix::stream_descriptor> ready_descriptor_;
    bonjour_shared_connection shared_connection_;
    std::map<std::string, bonjour_scoped_dns_service_ref> browsers_;
    std::map<std::string, service> services_;
    std::recursive_mutex lock_;

    /**
     * Called by dns_sd logic in response to a browse reply.
     * @param browse_service_ref The DNSServiceRef.
     * @param flags Possible values are kDNSServiceFlagsMoreComing and kDNSServiceFlagsAdd.
     *                See flag definitions for details.
     * @param interface_index The interface on which the service is advertised. This index should be passed to
     *                       DNSServiceResolve() when resolving the service.
     * @param error_code Will be kDNSServiceErr_NoError (0) on success, otherwise will indicate the failure that
     *                  occurred. Other parameters are undefined if the errorCode is nonzero.
     * @param name The discovered service name. This name should be displayed to the user, and stored for subsequent use
     *             in the DNSServiceResolve() call.
     * @param type The service type.
     * @param domain The domain of the discovered service instance.
     * @param context The user data.
     */
    static void browse_reply(
        DNSServiceRef browse_service_ref, DNSServiceFlags flags, uint32_t interface_index,
        DNSServiceErrorType error_code, const char* name, const char* type, const char* domain, void* context
    );

    void process_result();
};

}  // namespace rav::dnssd

#endif
