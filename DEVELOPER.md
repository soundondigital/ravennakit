# Notes for developers

## Conventions

### Include order

Include headers in the following order:

- Local project headers
- External dependency headers
- Standard headers

Each block separated by a space (to not have the include order changed by clang-format).

## Guidelines for exception safety and error handling in the library

1. **Ensure exception-safe code**

   - All code within the library must be exception-safe.
   - Resources must be managed using RAII (Resource Acquisition Is Initialization) or equivalent techniques to guarantee proper cleanup, even in the presence of exceptions.
   - Assume that any non-`noexcept` function can throw an exception.
   - Be aware that many components of the C++ standard library and other third-party libraries rely on exceptions for error handling, making it impractical to avoid exceptions entirely.

2. **Use both return values and exceptions for error handling**

   - Use return values wherever possible to signal errors. For example, leverage `tl::expected` to encapsulate the result of an operation that might fail.
      - `tl::expected` allows the caller to handle errors explicitly as return values, providing a more flexible mechanism for error handling.
      - This approach gives the caller the choice to process errors using custom logic or convert them into exceptions when appropriate.
   - Throw exceptions where return values are not feasible, such as in constructors.
      - A constructor should throw an exception if it fails to establish the invariants required for an object’s proper functionality.
      - Ensure any partially initialized resources are cleaned up properly to avoid leaks when an exception occurs.

3. **Prioritize explicit error handling**

   - Make error handling as explicit as possible, ensuring that the code’s behavior in error scenarios is predictable and clear.
   - Avoid relying on exceptions solely because of stack unwinding. Instead, use exceptions to signal critical failures that cannot be represented with return values.
   - Provide clear and detailed documentation for all error-handling paths, whether they involve return values or exceptions.

4. **Avoid using exceptions or return values for control flow**

   - Both exceptions and error return values must be reserved for truly exceptional circumstances. They should not be used to control routine program logic or handle non-critical events.
   - Examples of misuse:
      - Using exceptions for looping conditions or state transitions.
      - Using error codes for predictable outcomes (e.g., checking if a file exists).
   - Ensure normal operations are handled with predictable, clear, and performant logic without relying on exceptions or error propagation mechanisms.

## Quick commands

### Send audio as RTP stream example command

    ffmpeg -re -stream_loop -1 -f s16le -ar 48000 -ac 2 -i Sin420Hz@0dB16bit48kHzS.wav -c:a pcm_s16be -f rtp -payload_type 10 rtp://127.0.0.1:5004

## Resources

### RTSP

RTSP Test tool (also has a very good explanation of RTSP):  
[RTSPTest](https://github.com/rayryeng/RTSPTest)

### SDP

sdpoker test tool:  
[sdpoker](https://github.com/AMWA-TV/sdpoker)

### C++ web servers

https://github.com/oatpp/oatpp
https://github.com/civetweb/civetweb  
https://github.com/drogonframework/drogon  
https://github.com/uNetworking/uWebSockets  
https://github.com/Stiffstream/restinio  
https://github.com/CrowCpp/Crow  
https://github.com/pistacheio/pistache  (only Linux and macOS)

### Sample rate converter libraries

Audio Resampler:  
https://github.com/dbry/audio-resampler  
Offers dynamic ratio for asrc purposes and provides methods for getting the internal state (phase) for precise
resampling.

Secret Rabbit Code (libsamplerate):  
https://github.com/libsndfile/libsamplerate  
Offers dynamic ratio for asrc purposes but does not provide methods for getting the internal state (phase) for precise
resampling.
