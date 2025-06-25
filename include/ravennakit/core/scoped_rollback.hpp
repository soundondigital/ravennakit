/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "log.hpp"

#include <functional>
#include <vector>

namespace rav {

/**
 * A class that holds a rollback function, calling it upon destruction unless a commit is made.
 * This serves as an alternative to the 'goto cleanup' pattern in C, providing a mechanism to roll back changes
 * if subsequent operations fail. The class is exception-safe if given function is exception safe.
 */
class ScopedRollback {
  public:
    /**
     * Default constructor that initializes an empty rollback object.
     */
    ScopedRollback() = default;

    /**
     * Constructs a rollback object with an initial rollback function.
     * @param rollback_function The function to execute upon destruction if not committed.
     */
    explicit ScopedRollback(std::function<void()>&& rollback_function) {
        rollback_function_ = std::move(rollback_function);
    }

    /**
     * Destructor that executes all registered rollback functions if not committed.
     */
    ~ScopedRollback() {
        if (rollback_function_) {
            try {
                rollback_function_();
            } catch (const std::exception& e) {
                RAV_ERROR("Exception caught: {}", e.what());
            }
        }
    }

    ScopedRollback(const ScopedRollback&) = delete;
    ScopedRollback& operator=(const ScopedRollback&) = delete;

    ScopedRollback(ScopedRollback&&) = delete;
    ScopedRollback& operator=(ScopedRollback&&) = delete;

    /**
     * Commits the rollback, clears the stored function.
     * Call this when the rollback is no longer needed.
     */
    void commit() {
        rollback_function_ = {};
    }

  private:
    std::function<void()> rollback_function_;
};

}  // namespace rav
