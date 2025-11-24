# Getting started

## Integrating using CMake

The easiest and recommended way of integration RAVENNAKIT into your project is by using CMake. The following steps are
required:

1. Copy the RAVENNAKIT source code into the source tree of your project.
2. Make the dependencies available through `find_package()` as RAVENNAKIT will try to link the dependencies using the
   CMake `find_package()` command. Recommended is to use vcpkg. See [dependencies](dependencies.md) for more details.
3. Call add_subdirectory() in your CMakeLists.txt file.:

```cmake
add_subdirectory(path/to/ravennakit)
``` 

Then call target_link_libraries() to link your target against the RAVENNAKIT library:

```cmake
target_link_libraries(your_target PRIVATE ravennakit)
```

## Manually integrating RAVENNAKIT

It is possible to manually integrate RAVENNAKIT into your project. This requires some manual steps and is not as trivial
as the CMake approach:

1. Copy the RAVENNAKIT source code into the source tree of your project.
2. Add the RAVENNAKIT source files to your project (path/to/ravennakit/include/** and path/to/ravennakit/src/**)
3. Link the required dependencies to your project. Visit the [dependencies](dependencies.md) documentation for more details.

## Build configurations and options

To influence how RAVENNAKIT is built, several variables can be set. Head over to the [options](options.md)
documentation for more details.

## Setting up a RavennaNode

The easiest way to get started is to use the `rav::RavennaNode` class. This class offers the highest available 
abstraction of a RAVENNA node and provides a simple API to set up streams and to configure the node. It basically acts 
like a virtual RAVENNA node. Using this class also makes it easier to deal with cross thread boundaries.

The following code snippet shows how to set up a RAVENNA node:

```cpp
#include "ravennakit/ravenna/ravenna_node.hpp"
#include "ravennakit/core/system.hpp"

namespace examples {

class RavennaNodeSubscriber: public rav::RavennaNode::Subscriber {
  public:
    void ravenna_node_discovered(const rav::dnssd::ServiceDescription& desc) override {
        RAV_LOG_INFO("Discovered node: {}", desc.to_string());
    }

    void ravenna_node_removed(const rav::dnssd::ServiceDescription& desc) override {
        RAV_LOG_INFO("Removed node: {}", desc.to_string());
    }

    void ravenna_session_discovered(const rav::dnssd::ServiceDescription& desc) override {
        RAV_LOG_INFO("Discovered session: {}", desc.to_string());
    }

    void ravenna_session_removed(const rav::dnssd::ServiceDescription& desc) override {
        RAV_LOG_INFO("Removed session: {}", desc.to_string());
    }

    void ravenna_node_configuration_updated(const rav::RavennaNode::Configuration& configuration) override {
        RAV_LOG_INFO("Node configuration updated: {}", rav::to_string(configuration));
    }

    void ravenna_receiver_added([[maybe_unused]] const rav::RavennaReceiver& receiver) override {
        // Called when a receiver was added to the node.
    }

    void ravenna_receiver_removed([[maybe_unused]] const rav::Id receiver_id) override {
        // Called when a receiver was removed from the node.
    }

    void ravenna_sender_added([[maybe_unused]] const rav::RavennaSender& sender) override {
        // Called when a sender was added to the node.
    }

    void ravenna_sender_removed([[maybe_unused]] const rav::Id sender_id) override {
        // Called when a sender was removed fom the node.
    }

    void nmos_node_config_updated([[maybe_unused]] const rav::nmos::Node::Configuration& config) override {
        // Called when the NMOS node configuration was updated.
    }

    void nmos_node_status_changed(
        [[maybe_unused]] const rav::nmos::Node::Status status, [[maybe_unused]] const rav::nmos::Node::StatusInfo& registry_info
    ) override {
        // Called when the NMOS status changed (connected to a registry for example).
    }

    void network_interface_config_updated([[maybe_unused]] const rav::NetworkInterfaceConfig& config) override {
        // Called when the network interface config was updated.
    }
};

}  // namespace examples

/**
 * This examples demonstrates the steps to set up a RavennaNode. This example deliberately doesn't set up any senders or receivers to keep
 * things simple. To find out more have a look at the other examples.
 */
int main([[maybe_unused]] int const argc, [[maybe_unused]] char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    rav::RavennaNode::Configuration node_config;
    node_config.enable_dnssd_node_discovery = true;
    node_config.enable_dnssd_session_advertisement = true;
    node_config.enable_dnssd_session_discovery = true;

    rav::RavennaNode node;
    node.set_configuration(node_config);

    examples::RavennaNodeSubscriber subscriber;
    node.subscribe(&subscriber).wait();

    fmt::println("Press return key to stop...");
    std::string line;
    std::getline(std::cin, line);

    node.unsubscribe(&subscriber).wait();

    return 0;
}
``` 

For more detailed examples on how to set up a RAVENNA node, see [examples/](../examples).
