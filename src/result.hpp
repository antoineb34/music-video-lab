#ifndef MVLAB_RESULT_HPP
#define MVLAB_RESULT_HPP

#include "error.hpp"
#include <variant>
#include <optional>
#include <utility>

namespace mvlab {

// Result<T> holds exactly one of a successful T value or an Error. Built on
// std::variant so an invalid/mixed state is unrepresentable.
//
// Accessing value() on a failed Result (or error() on a successful one)
// throws std::bad_variant_access, matching std::variant's own contract.
// This keeps Result itself exception-free for expected operational
// failures (callers are expected to check has_value()/operator bool()
// first) while still failing loudly - rather than silently returning a
// default-constructed T - if a caller asks for the wrong alternative.
template <typename T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Error error) : storage_(std::move(error)) {}

    bool has_value() const { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const { return has_value(); }

    T& value() & { return std::get<T>(storage_); }
    const T& value() const& { return std::get<T>(storage_); }
    T&& value() && { return std::get<T>(std::move(storage_)); }

    Error& error() & { return std::get<Error>(storage_); }
    const Error& error() const& { return std::get<Error>(storage_); }

private:
    std::variant<T, Error> storage_;
};

// Result<void> cannot store a value in a std::variant, so it is
// represented as an optional Error: no error means success. error()
// throws std::bad_optional_access if called on a successful Result,
// mirroring the primary template's fail-loudly-on-misuse contract.
template <>
class Result<void> {
public:
    Result() : error_(std::nullopt) {}
    Result(Error error) : error_(std::move(error)) {}

    bool has_value() const { return !error_.has_value(); }
    explicit operator bool() const { return has_value(); }

    Error& error() & { return error_.value(); }
    const Error& error() const& { return error_.value(); }

private:
    std::optional<Error> error_;
};

} // namespace mvlab

#endif // MVLAB_RESULT_HPP
