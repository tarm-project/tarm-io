/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

#include "EventLoop.h"
#include "Export.h"

#include <memory>
#include <functional>

namespace tarm {
namespace io {

class TARM_IO_DLL_PUBLIC_CLASS_UNIX_ONLY Removable {
public:
    friend class RefCounted;

    // Used for smart pointers
    using DefaultDelete = std::function<void(Removable*)>;

    using OnScheduleRemovalCallback = std::function<void(const Removable&)>;

    TARM_IO_FORBID_COPY(Removable);
    TARM_IO_FORBID_MOVE(Removable);

    TARM_IO_DLL_PUBLIC Removable(EventLoop& loop);
    TARM_IO_DLL_PUBLIC virtual ~Removable();

    TARM_IO_DLL_PUBLIC virtual void schedule_removal();

    // Does nothing if removal was scheduled
    TARM_IO_DLL_PUBLIC void set_on_schedule_removal(const OnScheduleRemovalCallback& callback);

    TARM_IO_DLL_PUBLIC bool is_removal_scheduled() const;

    TARM_IO_DLL_PUBLIC static DefaultDelete default_delete();

protected:
    TARM_IO_DLL_PUBLIC void set_removal_scheduled();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io
} // namespace tarm
