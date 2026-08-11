#include <reverb/graph/HostPatchState.h>

#include <reverb/graph/PatchJson.h>

#include <exception>

namespace reverb::graph {

bool HostPatchState::store(const std::string_view patchJson, std::string& error)
{
    if (patchJson.size() > maximumSerializedBytes) {
        error = "patch state exceeds the 8 MiB host-state limit";
        return false;
    }

    try {
        static_cast<void>(parsePatchJson(patchJson));
    } catch (const std::exception& reason) {
        error = reason.what();
        return false;
    }

    {
        const std::scoped_lock lock(mutex_);
        patchJson_ = patchJson;
    }
    error.clear();
    return true;
}

void HostPatchState::clear()
{
    const std::scoped_lock lock(mutex_);
    patchJson_.reset();
}

std::optional<std::string> HostPatchState::snapshot() const
{
    const std::scoped_lock lock(mutex_);
    return patchJson_;
}

std::optional<GraphDocument> HostPatchState::document() const
{
    const auto copy = snapshot();
    return copy.has_value() ? std::optional { parsePatchJson(*copy) } : std::nullopt;
}

} // namespace reverb::graph
