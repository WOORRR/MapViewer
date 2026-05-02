#pragma once

#include "core/Messages.h"

namespace mv {

// A bus-wide monotonically increasing event id source plus a session id minted
// once per process. Both are thread safe.
class IdGen {
public:
    static SessionId session();           // stable for the process lifetime
    static EventId   next_event();        // monotonically increasing
};

}  // namespace mv
