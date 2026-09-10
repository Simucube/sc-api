#pragma once

#include <nanobind/nanobind.h>
#include <sc-api/session_fwd.h>

#include <type_traits>

#if NB_VERSION_MAJOR < 3 || (NB_VERSION_MAJOR == 3 && NB_VERSION_MINOR == 0 && NB_VERSION_PATCH < 1)
#error "sc-api Python bindings require nanobind 3.0.1 or newer"
#endif

namespace nb = nanobind;

namespace bind_util {

/// Detects a public weak_from_this() on T, exactly like nanobind's internal detection.
///
/// nanobind's shared_ptr caster runs the same test. When it succeeds the caster reuses the control
/// block that already owns the object instead of creating a second one. We keep our own copy so
/// that the static_asserts that guard this behaviour do not depend on nb::detail.
template <typename T>
auto            hasSharedFromThisImpl(T* ptr) -> decltype(ptr->weak_from_this().lock().get(), std::true_type{});
std::false_type hasSharedFromThisImpl(...);

template <typename T>
constexpr bool has_shared_from_this_v = decltype(hasSharedFromThisImpl((T*)nullptr))::value;

/// Keeps `patient` alive at least as long as `nurse`.
///
/// nanobind 3.0 removed the internal nb::detail::keep_alive free function. The public replacement
/// nb::keep_alive_obj arrives in nanobind 3.1. Until then we call the backend ABI slot that the
/// nb::keep_alive<Nurse, Patient> annotation itself uses (see nanobind/nb_attr.h). nanobind
/// declares those slots a frozen part of the versioned backend ABI.
inline void keepAlive(nb::handle nurse, nb::handle patient) {
#if NB_VERSION_MAJOR > 3 || (NB_VERSION_MAJOR == 3 && NB_VERSION_MINOR >= 1)
    nb::keep_alive_obj(nurse, patient);
#else
    NB_CALL(keep_alive_py)(NB_CTX, nurse.ptr(), patient.ptr());
#endif
}

}  // namespace bind_util

inline const char* session_state_str(sc_api::SessionState s) {
    switch (s) {
        case sc_api::SessionState::invalid:
            return "invalid";
        case sc_api::SessionState::connected_monitor:
            return "connected_monitor";
        case sc_api::SessionState::connected_control:
            return "connected_control";
        case sc_api::SessionState::session_lost:
            return "session_lost";
        default:
            return "unknown";
    }
}
