# Notes for developers

## Conventions

### Include order

Include headers in the following order:

- Local project headers
- External dependency headers
- Standard headers

Each block separated by a space (to not have the include order changed by clang-format).

### Exception policy

This library adopts exceptions as the primary mechanism for error handling. Exceptions provide a clean, structured way to manage error conditions, allowing the separation of normal code paths from error-handling logic. By utilizing exceptions, the library aims to improve code readability and maintainability, while ensuring robust error management.

All exceptions are used to signal error conditions, and the library encourages developers to handle exceptions at appropriate levels of abstraction. This ensures that resources are properly released and errors are dealt with in a way that minimizes disruption to the overall application.

Key principles for using exceptions:

Use exceptions for error conditions: Exceptions should be thrown when an operation fails and cannot continue normally. This allows the caller to handle the error without cluttering the code with manual error checks.

Avoid using exceptions for control flow: Exceptions should represent truly exceptional circumstances, not be used for routine operations or logic control. They should be reserved for scenarios where normal execution cannot proceed.

Ensure exception safety: All code must be exception-safe. Resources must be managed using RAII (Resource Acquisition Is Initialization) or other techniques that guarantee proper cleanup, even in the presence of exceptions. This ensures that no resources are leaked during error conditions.

Document thrown exceptions: Functions and methods should clearly document which exceptions may be thrown and under what conditions, allowing users of the library to understand potential error scenarios and handle them appropriately.

Catch exceptions where it makes sense: Exceptions should be caught and handled at a level where meaningful recovery can occur. If recovery is impossible, exceptions should propagate up to higher layers, where the application can decide how to respond.

In some cases, the library uses return values for error handling. This approach is appropriate when failures are expected and common, or when using exceptions would cause issues with stack unwinding. By returning error codes or status values, the library can handle scenarios where predictable failures occur without triggering the overhead of exception handling.
