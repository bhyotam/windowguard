#pragma once

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace guard {

struct Monitor {
    HMONITOR handle = nullptr;
    RECT bounds{};
    RECT work{};
    std::wstring device;
    std::wstring name;
    bool blocked = false;
    bool primary = false;
    std::vector<std::wstring> identities;
};

inline long Width(const RECT& rect) { return rect.right - rect.left; }
inline long Height(const RECT& rect) { return rect.bottom - rect.top; }

inline int64_t IntersectionArea(const RECT& first, const RECT& second) {
    const auto width = std::max(0L, std::min(first.right, second.right) - std::max(first.left, second.left));
    const auto height = std::max(0L, std::min(first.bottom, second.bottom) - std::max(first.top, second.top));
    return static_cast<int64_t>(width) * height;
}

inline bool TouchesBlocked(const RECT& rect, const std::vector<Monitor>& monitors) {
    for (const auto& monitor : monitors) {
        if (monitor.blocked && IntersectionArea(rect, monitor.bounds) > 0) return true;
    }
    return false;
}

inline const Monitor* PickDestination(const RECT& rect, const std::vector<Monitor>& monitors) {
    const Monitor* best = nullptr;
    int64_t bestArea = -1;
    int64_t bestDistance = std::numeric_limits<int64_t>::max();
    for (const auto& monitor : monitors) {
        if (monitor.blocked) continue;
        const auto area = IntersectionArea(rect, monitor.bounds);
        const int64_t horizontal = std::max({0L, monitor.bounds.left - rect.right, rect.left - monitor.bounds.right});
        const int64_t vertical = std::max({0L, monitor.bounds.top - rect.bottom, rect.top - monitor.bounds.bottom});
        const auto distance = horizontal * horizontal + vertical * vertical;
        if (area > bestArea || (area == bestArea && distance < bestDistance) ||
            (area == bestArea && distance == bestDistance && monitor.primary && best && !best->primary)) {
            best = &monitor;
            bestArea = area;
            bestDistance = distance;
        }
    }
    return best;
}

inline RECT FitInside(const RECT& rect, const RECT& work, bool resizable) {
    const long width = resizable ? std::min(Width(rect), Width(work)) : Width(rect);
    const long height = resizable ? std::min(Height(rect), Height(work)) : Height(rect);
    const long left = std::clamp(rect.left, work.left, std::max(work.left, work.right - width));
    const long top = std::clamp(rect.top, work.top, std::max(work.top, work.bottom - height));
    return {left, top, left + width, top + height};
}

inline RECT ToWorkspace(RECT rect, const Monitor& monitor, bool toolWindow) {
    if (!toolWindow) OffsetRect(&rect, monitor.bounds.left - monitor.work.left, monitor.bounds.top - monitor.work.top);
    return rect;
}

}
