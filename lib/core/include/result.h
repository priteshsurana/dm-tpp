#pragma once

#include <variant>
#include <functional>
#include <optional>
#include <stdexcept>

namespace dmtpp {
namespace core {

// Forward declare Error
struct Error;

// Result<T, E> type for explicit error handling
// Inspired by Rust's Result type
template<typename T, typename E = Error>
class Result {
public:
    // Construct successful result
    static Result Ok(T value) {
        return Result(std::move(value));
    }
    
    // Construct error result
    static Result Err(E error) {
        return Result(std::move(error));
    }
    
    // Check if contains value
    bool is_ok() const {
        return std::holds_alternative<T>(data_);
    }
    
    // Check if contains error
    bool is_err() const {
        return std::holds_alternative<E>(data_);
    }
    
    // Unwrap value (throws if error)
    // Use sparingly; prefer pattern matching
    T unwrap() {
        if (is_err()) {
            throw std::runtime_error("Called unwrap on Err value");
        }
        return std::get<T>(std::move(data_));
    }
    
    // Unwrap value or return default
    T unwrap_or(T default_value) {
        if (is_ok()) {
            return std::get<T>(std::move(data_));
        }
        return default_value;
    }
    
    // Unwrap value or compute from error
    T unwrap_or_else(std::function<T(E)> func) {
        if (is_ok()) {
            return std::get<T>(std::move(data_));
        }
        return func(std::get<E>(data_));
    }
    
    // Get error (throws if ok)
    E error() {
        if (is_ok()) {
            throw std::runtime_error("Called error() on Ok value");
        }
        return std::get<E>(std::move(data_));
    }
    
    // Map: transform Ok value, pass through Err
    template<typename U>
    Result<U, E> map(std::function<U(T)> func) {
        if (is_ok()) {
            return Result<U, E>::Ok(func(std::get<T>(data_)));
        }
        return Result<U, E>::Err(std::get<E>(data_));
    }
    
    // And_then: chainable transformation (flatMap)
    template<typename U>
    Result<U, E> and_then(std::function<Result<U, E>(T)> func) {
        if (is_ok()) {
            return func(std::get<T>(data_));
        }
        return Result<U, E>::Err(std::get<E>(data_));
    }
    
    // Map_err: transform Err value, pass through Ok
    template<typename F>
    Result<T, F> map_err(std::function<F(E)> func) {
        if (is_err()) {
            return Result<T, F>::Err(func(std::get<E>(data_)));
        }
        return Result<T, F>::Ok(std::get<T>(data_));
    }
    
private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(E error) : data_(std::move(error)) {}
    
    std::variant<T, E> data_;
};

// Specialization for void (Result<void, E>)
template<typename E>
class Result<void, E> {
public:
    static Result Ok() {
        return Result(true);
    }
    
    static Result Err(E error) {
        return Result(std::move(error));
    }
    
    bool is_ok() const { return is_ok_; }
    bool is_err() const { return !is_ok_; }
    
    void unwrap() {
        if (!is_ok_) {
            throw std::runtime_error("Called unwrap on Err value");
        }
    }
    
    E error() {
        if (is_ok_) {
            throw std::runtime_error("Called error() on Ok value");
        }
        return std::move(error_.value());
    }
    
    template<typename U>
    Result<U, E> and_then(std::function<Result<U, E>()> func) {
        if (is_ok_) {
            return func();
        }
        return Result<U, E>::Err(error_.value());
    }
    
private:
    explicit Result(bool ok) : is_ok_(ok) {}
    explicit Result(E error) : is_ok_(false), error_(std::move(error)) {}
    
    bool is_ok_;
    std::optional<E> error_;
};

} // namespace core
} // namespace dmtpp