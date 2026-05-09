#pragma once

// A minimal Result<T, E> type used as a sum-type for fallible operations.
// Mirrors the Rust `Result<T, &str>` style used in the original codebase.
//
// We intentionally do not depend on tl::expected or std::expected so this
// header stays buildable on every toolchain in our matrix (clang 14+, GCC 11+).

#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace ccm {

template <typename T, typename E = std::string>
class Result {
public:
    using value_type = T;
    using error_type = E;

    static Result ok(T value) { return Result(OkTag{}, std::move(value)); }
    static Result err(E error) { return Result(ErrTag{}, std::move(error)); }

    Result(const Result& other) : has_value_(other.has_value_) {
        if (has_value_) {
            ::new (static_cast<void*>(&value_)) T(other.value_);
        } else {
            ::new (static_cast<void*>(&error_)) E(other.error_);
        }
    }

    Result(Result&& other) noexcept : has_value_(other.has_value_) {
        if (has_value_) {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
        } else {
            ::new (static_cast<void*>(&error_)) E(std::move(other.error_));
        }
    }

    Result& operator=(const Result& other) {
        if (this == &other) return *this;
        destroy();
        has_value_ = other.has_value_;
        if (has_value_) {
            ::new (static_cast<void*>(&value_)) T(other.value_);
        } else {
            ::new (static_cast<void*>(&error_)) E(other.error_);
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept {
        if (this == &other) return *this;
        destroy();
        has_value_ = other.has_value_;
        if (has_value_) {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
        } else {
            ::new (static_cast<void*>(&error_)) E(std::move(other.error_));
        }
        return *this;
    }

    ~Result() { destroy(); }

    [[nodiscard]] bool isOk() const noexcept { return has_value_; }
    [[nodiscard]] bool isErr() const noexcept { return !has_value_; }
    explicit operator bool() const noexcept { return has_value_; }

    const T& value() const& {
        if (!has_value_) throw std::logic_error("Result::value() on err");
        return value_;
    }
    T& value() & {
        if (!has_value_) throw std::logic_error("Result::value() on err");
        return value_;
    }
    T&& value() && {
        if (!has_value_) throw std::logic_error("Result::value() on err");
        return std::move(value_);
    }

    const E& error() const& {
        if (has_value_) throw std::logic_error("Result::error() on ok");
        return error_;
    }
    E&& error() && {
        if (has_value_) throw std::logic_error("Result::error() on ok");
        return std::move(error_);
    }

    template <typename U>
    T valueOr(U&& fallback) const& {
        return has_value_ ? value_ : static_cast<T>(std::forward<U>(fallback));
    }

private:
    struct OkTag {};
    struct ErrTag {};

    Result(OkTag, T value) : has_value_(true) {
        ::new (static_cast<void*>(&value_)) T(std::move(value));
    }
    Result(ErrTag, E error) : has_value_(false) {
        ::new (static_cast<void*>(&error_)) E(std::move(error));
    }

    void destroy() noexcept {
        if (has_value_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }

    bool has_value_;
    union {
        T value_;
        E error_;
    };
};

// Specialization for void-returning fallible operations.
template <typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    static Result ok() { return Result(true, E{}); }
    static Result err(E error) { return Result(false, std::move(error)); }

    [[nodiscard]] bool isOk() const noexcept { return has_value_; }
    [[nodiscard]] bool isErr() const noexcept { return !has_value_; }
    explicit operator bool() const noexcept { return has_value_; }

    const E& error() const& {
        if (has_value_) throw std::logic_error("Result::error() on ok");
        return error_;
    }

private:
    Result(bool ok, E error) : has_value_(ok), error_(std::move(error)) {}
    bool has_value_;
    E    error_;
};

}  // namespace ccm
