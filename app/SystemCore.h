#pragma once

/**
 * @brief A base class for a core's main controller using the CRTP pattern
 * to eliminate virtual function overhead.
 * 
 * @tparam Derived The concrete core controller class (e.g., Core0SystemCore).
 */
template<typename Derived>
class SystemCore {
public:
    /**
     * @brief Public-facing init function. Calls the derived class's implementation.
     */
    bool init() {
        return static_cast<Derived*>(this)->init_impl();
    }

    /**
     * @brief Public-facing loop function. Calls the derived class's implementation.
     */
    void loop() {
        static_cast<Derived*>(this)->loop_impl();
    }

protected:
    // Ensure this class can only be used as a base.
    SystemCore() = default;
    ~SystemCore() = default;
};
