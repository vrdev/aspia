//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef DESKTOP__CAPTURE_SCHEDULER_H
#define DESKTOP__CAPTURE_SCHEDULER_H

#include <chrono>

#include "base/macros_magic.h"

namespace desktop {

class CaptureScheduler
{
public:
    explicit CaptureScheduler(const std::chrono::milliseconds& update_interval);
    ~CaptureScheduler() = default;

    void beginCapture();
    void endCapture();
    std::chrono::milliseconds nextCaptureDelay() const;

private:
    std::chrono::milliseconds update_interval_;
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;

    DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

} // namespace desktop

#endif // DESKTOP__CAPTURE_SCHEDULER_H
