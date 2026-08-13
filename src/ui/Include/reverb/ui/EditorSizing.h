#pragma once

#include <compare>

namespace reverb::ui {

struct EditorSize {
    int width;
    int height;

    auto operator<=>(const EditorSize&) const = default;
};

inline EditorSize preferredEditorSize(
    const bool, const int, const int)
{
    constexpr EditorSize hostedPreferred { 1280, 800 };
    return hostedPreferred;
}

} // namespace reverb::ui
