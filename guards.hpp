#pragma once

#include <Windows.h>
#include <concepts>

class HandleGuard
{
    HANDLE m_handle{ INVALID_HANDLE_VALUE };

public:
    HandleGuard(const HANDLE handle):
        m_handle(handle)
    {}
    ~HandleGuard()
    {
        if (!m_handle || m_handle == INVALID_HANDLE_VALUE) return;
        CloseHandle(m_handle);
    }

    HANDLE get() const noexcept
    { return m_handle; }
};

template<typename T>
concept HasRelease = requires(T t)
{
    { t.Release() };
};

template<HasRelease T>
class UiaGuard
{
    T *m_element;

public:
    UiaGuard(const UiaGuard &) = delete;
    UiaGuard& operator=(const UiaGuard&) = delete;

    UiaGuard(T *element): m_element(element) {}

    UiaGuard(UiaGuard &&other) noexcept:
        m_element(other.m_element)
    {
        other.m_element = nullptr;
    }

    UiaGuard &operator=(UiaGuard &&other) noexcept
    {
        if (this != &other)
        {
            if (m_element) m_element->Release();

            m_element = other.m_element;
            other.m_element = nullptr;
        }

        return *this;
    }

    ~UiaGuard()
    {
        if (!m_element) return;
        m_element->Release();
    }

    T **ptr() noexcept { return &m_element; }
    T *value() noexcept { return m_element; }
};
