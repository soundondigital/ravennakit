#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

#include "bonjour.hpp"
#include "bonjour_process_results_thread.hpp"

#include <boost/asio/io_context.hpp>

#if RAV_HAS_APPLE_DNSSD

    #include "bonjour_scoped_dns_service_ref.hpp"
    #include "bonjour_shared_connection.hpp"
    #include "ravennakit/dnssd/dnssd_browser.hpp"
    #include "ravennakit/core/platform/posix/pipe.hpp"

namespace rav::dnssd {

/**
 * Apple Bonjour implementation of IBrowser. Works on macOS and Windows.
 */
class BonjourBrowser: public Browser {
  public:
    /**
     * Represents a Bonjour service and holds state and methods for discovering and resolving services on the network.
     */
    class Service {
      public:
        /**
         * Constructs a service.
         * @param fullname Full domain name of the service.
         * @param name Name of the service.
         * @param type Type of the service (i.e. _http._tcp)
         * @param domain The domain of the service (i.e. local.).
         * @param owner A reference to the owning BonjourBrowser.
         */
        Service(const char* fullname, const char* name, const char* type, const char* domain, BonjourBrowser& owner);

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
        [[nodiscard]] const ServiceDescription& description() const noexcept;

      private:
        BonjourBrowser& owner_;
        std::map<uint32_t, BonjourScopedDnsServiceRef> resolvers_;
        std::map<uint32_t, BonjourScopedDnsServiceRef> get_addrs_;
        ServiceDescription description_;

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
         * @param context
         */
        static void resolve_callback(
            DNSServiceRef service_ref, DNSServiceFlags flags, uint32_t interface_index, DNSServiceErrorType error_code,
            const char* fullname, const char* host_target, uint16_t port,
            // In network byte order.
            uint16_t txt_len, const unsigned char* txt_record, void* context
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
         * @param context
         */
        static void get_addr_info_callback(
            DNSServiceRef sd_ref, DNSServiceFlags flags, uint32_t interface_index, DNSServiceErrorType error_code,
            const char* hostname, const struct sockaddr* address, uint32_t ttl, void* context
        );
    };

    explicit BonjourBrowser(boost::asio::io_context& io_context);
    void browse_for(const std::string& service) override;
    [[nodiscard]] const ServiceDescription* find_service(const std::string& service_name) const override;
    [[nodiscard]] std::vector<ServiceDescription> get_services() const override;

  private:
    boost::asio::ip::tcp::socket service_socket_;
    BonjourSharedConnection shared_connection_;
    std::map<std::string, Service> services_;                         // fullname -> service
    std::map<std::string, BonjourScopedDnsServiceRef> browsers_;  // reg_type -> DNSServiceRef
    size_t process_results_failed_attempts_ = 0;

    void async_process_results();

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

    /**
     * Emits given event to all subscribers.
     * @tparam T The type of the event.
     * @param event The event to emit.
     */
    template<class T>
    void emit(T&& event) {
        event_emitter_.emit(std::forward<T>(event));
    }
};

}  // namespace rav::dnssd

#endif
