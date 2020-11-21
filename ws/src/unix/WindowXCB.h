//
// WS
// WindowXCB.h
//
// Copyright © 2020 Gustavo C. Viegas.
//

#ifndef YF_WS_WINDOWXCB_H
#define YF_WS_WINDOWXCB_H

#include "Window.h"
#include "XCB.h"

WS_NS_BEGIN

class WindowXCB final : public Window {
 public:
  WindowXCB(uint32_t width, uint32_t height, CreationMask mask);
  ~WindowXCB();
  void close();
  void toggleFullscreen();
  void resize(uint32_t, uint32_t);
  void show();
  void hide();
  uint32_t width() const;
  uint32_t height() const;

 private:
  xcb_window_t window_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  CreationMask mask_ = 0;
  bool fullscreen_ = false;
};

WS_NS_END

#endif // YF_WS_WINDOWXCB_H
