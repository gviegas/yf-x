//
// WS
// Event.h
//
// Copyright © 2020 Gustavo C. Viegas.
//

#ifndef YF_WS_EVENT_H
#define YF_WS_EVENT_H

#include "yf/ws/Keyboard.h"

WS_NS_BEGIN

/// Event manager.
///
class Event {
 public:
  virtual ~Event();

  /// Gets the event instance.
  ///
  static Event& get();

  /// Dispatches events.
  ///
  virtual void dispatch() = 0;

  /// Sets a delegate to handle keyboard events.
  ///
  virtual void setDelegate(const KbDelegate& delegate) = 0;
};

WS_NS_END

#endif // YF_WS_EVENT_H
