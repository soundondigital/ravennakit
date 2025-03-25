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

## Setting up a ravenna_node

The easiest way to get started is to use the `rav::ravenna_node` class. This class offers the highest available 
abstraction of a RAVENNA node and provides a simple API to set up streams and to configure the node. It basically acts 
like a virtual RAVENNA node. Using this class also makes it easier to deal with cross thread boundaries.

The following code snippet shows how to set up a RAVENNA node:

```cpp
struct ravenna_node_subscriber final: rav::ravenna_node::subscriber, rav::rtp::rtp_stream_receiver::subscriber {
    void ravenna_node_discovered(const rav::dnssd::dnssd_browser::service_resolved& event) override {
        RAV_INFO("RAVENNA node discovered: {}", event.description.to_string());
    }

    void ravenna_node_removed(const rav::dnssd::dnssd_browser::service_removed& event) override {
        RAV_INFO("RAVENNA node removed: {}", event.description.to_string());
    }

    void ravenna_session_discovered(const rav::dnssd::dnssd_browser::service_resolved& event) override {
        RAV_INFO("RAVENNA session discovered: {}", event.description.to_string());
    }

    void ravenna_session_removed(const rav::dnssd::dnssd_browser::service_removed& event) override {
        RAV_INFO("RAVENNA session removed: {}", event.description.to_string());
    }

    void ravenna_receiver_added(const rav::ravenna_receiver& receiver) override {
        RAV_INFO("RAVENNA receiver added for: {}", receiver.get_session_name());
    }

    void rtp_stream_receiver_updated(const rav::rtp::rtp_stream_receiver::stream_updated_event& event) override {
        RAV_INFO("Stream updated: {}", event.to_string());
    }
};

int main() {
    rav::log::set_level_from_env();
    rav::do_system_checks();

    rav::rtp::rtp_receiver::configuration config;
    config.interface_address = asio::ip::make_address("192.168.1.1");  // Fill in the address of the NIC to use

    rav::ravenna_node ravenna_node(config);
    ravenna_node_subscriber node_subscriber;

    ravenna_node.subscribe(&node_subscriber).wait();

    ravenna_node.create_receiver("session_name").wait();  // Fill in the RAVENNA session name to receive

    fmt::println("Press return key to stop...");
    std::cin.get();

    ravenna_node.unsubscribe(&node_subscriber).wait();

    return 0;
}
``` 

For a more detailed example on how to set up a RAVENNA node, see [ravenna_node_example.cpp](#ravenna_node_example.cpp).
