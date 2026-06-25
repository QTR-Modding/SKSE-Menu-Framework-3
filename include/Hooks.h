#pragma once
namespace Hooks {

    void Install();
    void InstallInputHooks();

    struct WndProcHook {
        static LRESULT thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static inline WNDPROC func;
    };

    struct D3DInitHook {
        static void thunk();
        static void install();
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    template <class T>
    struct InputHandlerHook : public T {
        using CanProcess_t = decltype(&T::CanProcess);
        static inline REL::Relocation<CanProcess_t> originalCanProcess;

        bool CanProcess_Hook(RE::InputEvent* a_event);

        static void Install(REL::Relocation<std::uintptr_t> a_vtbl, std::size_t a_offset) {
            originalCanProcess = a_vtbl.write_vfunc(a_offset, &CanProcess_Hook);
        }
    };

}