# Notes for developers

## Conventions

### Include order

Include headers in the following order:

- Local project headers
- External dependency headers
- Standard headers

Each block separated by a space (to not have the include order changed by clang-format).

### Exception policy

This library uses return values to indicate errors, reserving exceptions only for unrecoverable conditions. The only exceptions that may be thrown originate from underlying libraries.

All code must be exception-safe, meaning that all resources must be managed using RAII (Resource Acquisition Is Initialization) to ensure proper cleanup in the event of an exception. While exceptions are considered exceptional cases and often indicative of bugs—where terminating the program might be acceptable—they are propagated to the user to provide an opportunity for handling them.

To summarize:

- An exception should never occur under normal operation.
- If an exception is thrown, it is considered a bug, and terminating the program is acceptable.
- Use return values for error handling.
