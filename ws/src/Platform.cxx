//
// WS
// Platform.cxx
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#include "Platform.h"
#include "yf/Except.h"

#if defined(__linux__) // TODO: Other unix systems
//# include "unix/WindowWL.h"
//# include "unix/EventWL.h"
# include "unix/WindowXCB.h"
# include "unix/EventXCB.h"
#elif defined(__APPLE__)
# include "macos/WindowMAC.h"
# include "macos/EventMAC.h"
#elif defined(_WIN32)
# include "win32/WindowW32.h"
# include "win32/EventW32.h"
#else
# error "Invalid platform"
#endif // defined(__linux__)

using namespace YF_NS;
using namespace WS_NS;
using namespace std;

INTERNAL_NS_BEGIN

/// The current platform.
///
auto curPfm = WS_NS::PlatformNone;

/// The dispatch function.
///
void dispatchDummy();
function<void ()> dispatchFn = dispatchDummy;

/// Sets the platform-dependent dispatch function.
///
void dispatchDummy() {
#if defined(__linux__)
  switch (platform()) {
  case PlatformNone:
    throw UnsupportedExcept("No supported platform available");
  case PlatformXCB:
    dispatchFn = dispatchXCB;
    break;
  default:
    throw runtime_error("Unexpected");
  }
#else
// TODO: Other systems
# error "Unimplemented"
#endif // defined(__linux__)

  dispatchFn();
}

INTERNAL_NS_END

WS_NS_BEGIN

/// Sets the current platform.
///
/// System-specific `init*()` will call this function.
///
void setPlatform(Platform pfm) {
  curPfm = pfm;
}

Platform platform() {
  // Try to initialize a platform if have none
  if (curPfm == PlatformNone) {
#if defined(__linux__)
    if (getenv("WAYLAND_DISPLAY"))
      // TODO: Replace with `initWL` when implemented
      initXCB();
    else if (getenv("DISPLAY"))
      initXCB();
#else
// TODO: Other systems
# error "Unimplemented"
#endif // defined(__linux__)
  }

  return curPfm;
}

Window::Ptr createWindow(uint32_t width, uint32_t height, const wstring& title,
                         Window::CreationMask mask) {
#if defined(__linux__)
  switch (platform()) {
  case PlatformNone:
    throw UnsupportedExcept("No supported platform available");
  case PlatformXCB:
    return make_unique<WindowXCB>(width, height, title, mask);
  default:
    throw runtime_error("Unexpected");
  }
#else
// TODO: Other systems
# error "Unimplemented"
#endif // defined(__linux__)

  return nullptr;
}

void dispatch() {
  dispatchFn();
}

#if defined(__linux__)
xcb_connection_t* connectionXCB() {
  if (curPfm != PlatformXCB)
    throw runtime_error("XCB is not the current platform");

  return varsXCB().connection;
}

xcb_visualid_t visualIdXCB() {
  if (curPfm != PlatformXCB)
    throw runtime_error("XCB is not the current platform");

  return varsXCB().visualId;
}

xcb_window_t windowXCB(const Window& window) {
  if (curPfm != PlatformXCB)
    throw runtime_error("XCB is not the current platform");

  return static_cast<const WindowXCB&>(window).window();
}
#else
// TODO: Other systems
#endif // defined(__linux__)

WS_NS_END
