#pragma once

#include <algorithm>
#include <compare>

namespace reverb::ui {

struct EditorSize {
    int width;
    int height;

    auto operator<=>(const EditorSize&) const = default;
};

inline EditorSize preferredEditorSize(
    const bool standalone, const int workAreaWidth, const int workAreaHeight)
{
    constexpr EditorSize hostedPreferred { 1280, 800 };
    constexpr EditorSize minimum { 640, 400 };
    constexpr EditorSize workAreaMargin { 32, 64 };
    if (!standalone || workAreaWidth <= 0 || workAreaHeight <= 0)
        return hostedPreferred;
    return {
        std::clamp(workAreaWidth - workAreaMargin.width, minimum.width, 8192),
        std::clamp(workAreaHeight - workAreaMargin.height, minimum.height, 8192),
    };
}

} // namespace reverb::ui
