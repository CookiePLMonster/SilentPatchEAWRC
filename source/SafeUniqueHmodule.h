#pragma once

#include <wil/resource.h>

// Convenience wrapper for wil::unique_hmodule, so we let it leak if the interface it wraps is still referenced somewhere
class SafeUniqueHmodule final
{
public:
    SafeUniqueHmodule(wil::unique_hmodule module, wil::com_ptr_nothrow<IUnknown> instance)
        : m_module(std::move(module)), m_instance(instance.detach())
    {
    }

    ~SafeUniqueHmodule()
    {
        if (m_instance != nullptr)
        {
            const ULONG ref = m_instance->Release();
            if (ref != 0)
            {
                // Let the module leak
                m_module.release();
            }
        }
    }

private:
    wil::unique_hmodule m_module;
    IUnknown* m_instance;
};
