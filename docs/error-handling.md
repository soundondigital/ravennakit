# Error handling

## Strategy

Error handling in C++ is a much debated topic and there are many ways to do it. The canonical and modern approach is to
use exceptions, but there are reasons to not use exceptions and instead use return values. One of the reasons is that
exceptions hide a lot of control flow which makes it significantly harder to reason about the flow of execution in case
something goes wrong or something unexpected happens. Unfortunately it's not very ergonomic to avoid exceptions
completely, for example indicating an error condition from inside a constructor without an exception is impossible and a
lot of the standard library uses exceptions for all kinds of things.

RAVENNAKIT uses a hybrid approach, where it tries to make error handling and potential error cases as explicit as
possible by working with std::expected (or in fact tl::expected to support C++17). This helps the reader to understand 
the flow of execution for error conditions and turn return values into exceptions if preferred.

We use the following guidelines for developing the library:

1. **Ensure exception-safe code**

    - All code within the library must be exception-safe.
    - Resources must be managed using RAII (Resource Acquisition Is Initialization) or equivalent techniques to
      guarantee proper cleanup, even in the presence of exceptions.
    - Assume that any non-`noexcept` function may throw an exception.
    - Be aware that many components of the C++ standard library and other third-party libraries rely on exceptions for
      error handling, making it impractical to avoid exceptions entirely.

2. **Use both return values and exceptions for error handling**

    - Use return values wherever possible to signal errors. For example, leverage `tl::expected` to encapsulate the
      result of an operation that might fail.
        - `tl::expected` allows the caller to handle errors explicitly as return values, providing a more flexible
          mechanism for error handling.
        - This approach gives the caller the choice to process errors using custom logic or convert them into exceptions
          when appropriate.
    - Throw exceptions where return values are not feasible, such as in constructors.
        - A constructor should throw an exception if it fails to establish the invariants required for an object’s
          proper functionality.
        - Ensure any partially initialized resources are cleaned up properly to avoid leaks when an exception occurs.

3. **Prioritize explicit error handling**

    - Make error handling as explicit as possible, ensuring that the code’s behavior in error scenarios is predictable
      and clear. In practice this often means to use return values.
    - Avoid relying on exceptions solely because of stack unwinding. Instead, use exceptions to signal critical failures
      that cannot be represented with return values.
    - Provide clear and detailed documentation for all error-handling paths, whether they involve return values or
      exceptions.

4. **Avoid using exceptions or return values for control flow**

    - Both exceptions and error return values must be reserved for truly exceptional circumstances. They should not be
      used to control routine program logic or handle non-critical events.
    - Examples of misuse:
        - Using exceptions for looping conditions or state transitions.
        - Using error codes for predictable outcomes (e.g., checking if a file exists).
    - Ensure normal operations are handled with predictable, clear, and performant logic without relying on exceptions
      or error propagation mechanisms.

Note: these guidelines are not guarantees. Always consult the documentation of a function to understand how it behaves.

## Assertions

The RAVENNAKIT codebase makes use of assertions to catch problems early and to detect conditions that should never
occur. Assertions help enforce assumptions, aid in debugging, and increase overall code reliability.

RAVENNAKIT provides a set of custom assertion macros defined in assert.hpp:

| Macro               | Description                                                                                                                       |
|---------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| RAV_ASSERT          | Checks that a condition is true. Behavior on failure depends on compile-time settings (see below).                                |
| RAV_ASSERT_NO_THROW | Similar to RAV_ASSERT, but guarantees that no exception will be thrown, regardless of the value of RAV_THROW_EXCEPTION_ON_ASSERT. |
| RAV_ASSERT_FALSE    | Always fails. Useful for marking unreachable code paths or "should never happen" cases.                                           |

Assertion behavior can be customized using the following compile-time constants:

| Compile constant              | Description                                                                                                                                        |
|-------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|
| RAV_LOG_ON_ASSERT             | If enabled, a log message is emitted when an assertion fails.                                                                                      |
| RAV_THROW_EXCEPTION_ON_ASSERT | If enabled, a failed assertion throws an exception.                                                                                                |
| RAV_ABORT_ON_ASSERT           | If enabled, a failed assertion calls std::abort(). This is especially useful during debugging to immediately halt execution and inspect the state. |
