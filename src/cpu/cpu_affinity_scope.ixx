//
// Created by sexey on 04.01.2026.
//

module;
#include <windows.h>

export module radiance.process.affinity_scope;

export namespace radiance::process
{
    class C_CpuAffinityScope
    {
    public:

        ~C_CpuAffinityScope() noexcept
        {
            if (!this->isInitialized_) {
                return;
            }

            SetProcessAffinityMask(GetCurrentProcess(), this->originalMask_);
        }

        C_CpuAffinityScope() noexcept : originalMask_(0), isInitialized_(false) {}

        C_CpuAffinityScope(const C_CpuAffinityScope&) = delete;

        C_CpuAffinityScope& operator=(const C_CpuAffinityScope&) = delete;

        bool initialize() noexcept
        {
            DWORD_PTR systemMask = 0;
            if (!GetProcessAffinityMask(GetCurrentProcess(), &this->originalMask_, &systemMask)) {
                return false;
            }

            if (!this->originalMask_) {
                return false;
            }

            const auto targetMask = this->originalMask_ & -static_cast<intptr_t>(this->originalMask_);
            if (targetMask == 0) {
                return false;
            }

            if (!SetProcessAffinityMask(GetCurrentProcess(), targetMask)) {
                return false;
            }

            SwitchToThread();
            this->isInitialized_ = true;

            return true;
        }

    private:
        DWORD_PTR originalMask_;
        bool isInitialized_;
    };
}