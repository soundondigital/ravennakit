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

#include <functional>
#include <vector>

namespace rav {

/**
 * A class that manages a series of rollback functions, executing them upon destruction unless a commit is made.
 * This serves as an alternative to the 'goto cleanup' pattern in C, providing a mechanism to roll back changes
 * if subsequent operations fail. The class is exception-safe.
 */
class ScopedRollback {
  public:
    /**
     * Default constructor that initializes an empty rollback object.
     */
    ScopedRollback() = default;

    /**
     * Constructs a rollback object with an initial rollback function.
     * @param initial_rollback_function The function to execute upon destruction if not committed.
     */
    explicit ScopedRollback(std::function<void()>&& initial_rollback_function) {
        rollback_functions_.push_back(std::move(initial_rollback_function));
    }

    /**
     * Destructor that executes all registered rollback functions if not committed.
     */
    ~ScopedRollback() {
        for (auto& func : rollback_functions_) {
            if (func) {
                func();
            }
        }
    }

    ScopedRollback (const ScopedRollback&) = delete;
    ScopedRollback& operator= (const ScopedRollback&) = delete;

    /**
     * Adds a function to the rollback stack.
     * @param rollback_function The function to execute upon destruction if not committed.
     */
    void add (std::function<void()>&& rollback_function)
    {
        rollback_functions_.push_back (std::move (rollback_function));
    }

    /**
     * Commits the rollback, clearing all stored functions.
     * Call this when all operations have succeeded and no rollback is necessary.
     */
    void commit()
    {
        rollback_functions_.clear();
    }

  private:
    std::vector<std::function<void()>> rollback_functions_;
};

}  // namespace rav
