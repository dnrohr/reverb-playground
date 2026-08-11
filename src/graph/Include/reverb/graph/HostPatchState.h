#pragma once

#include <reverb/graph/GraphDocument.h>

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace reverb::graph {

class HostPatchState final {
public:
    static constexpr std::size_t maximumSerializedBytes = 8U * 1024U * 1024U;

    [[nodiscard]] bool store(std::string_view patchJson, std::string& error);
    void clear();
    [[nodiscard]] std::optional<std::string> snapshot() const;
    [[nodiscard]] std::optional<GraphDocument> document() const;

private:
    mutable std::mutex mutex_;
    std::optional<std::string> patchJson_;
};

} // namespace reverb::graph
