#pragma once

#include <compare>

namespace reverb::ui {

struct EditorSize {
    int width;
    int height;

    auto operator<=>(const EditorSize&) const = default;
};

inline EditorSize preferredEditorSize(
    const bool standalone, const int, const int)
{
    constexpr EditorSize hostedPreferred { 1280, 800 };
    constexpr EditorSize standalonePreferred { 1200, 720 };
    return standalone ? standalonePreferred : hostedPreferred;
}

} // namespace reverb::ui
