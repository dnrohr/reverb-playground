#include <reverb/dsp/BarrReference.h>
#include <reverb/dsp/BarrReferenceRuntime.h>

#include <reverb/dsp/Sum.h>

#include <algorithm>

namespace reverb::dsp {

void BarrReference::prepare(const double sampleRate)
{
    const auto prepareAllpass = [sampleRate](Allpass& allpass, const std::string_view nodeId) {
        allpass.prepare(
            sampleRate,
            barrReferenceParameter(nodeId, "delay"),
            static_cast<float>(barrReferenceParameter(nodeId, "coefficient")));
    };
    inputGain_.setLinear(0.5F);
    inputFilter_.prepare(sampleRate, barrReferenceParameter("input-filter", "cutoff"));
    prepareAllpass(diffuserOne_, "diffuser-1");
    prepareAllpass(diffuserTwo_, "diffuser-2");
    prepareAllpass(tankOne_, "tank-1");
    prepareAllpass(tankTwo_, "tank-2");
    prepareAllpass(leftTap_, "left-tap");
    prepareAllpass(rightTap_, "right-tap");
}

void BarrReference::reset() noexcept
{
    inputFilter_.reset();
    diffuserOne_.reset();
    diffuserTwo_.reset();
    tankOne_.reset();
    tankTwo_.reset();
    leftTap_.reset();
    rightTap_.reset();
}

void BarrReference::process(
    const std::span<const float> inputLeft,
    const std::span<const float> inputRight,
    const std::span<float> outputLeft,
    const std::span<float> outputRight,
    const float impulse) noexcept
{
    const auto count = std::min({ inputLeft.size(), inputRight.size(), outputLeft.size(), outputRight.size() });
    const auto left = outputLeft.first(count);
    const auto right = outputRight.first(count);

    Sum::process(inputLeft.first(count), inputRight.first(count), left);
    if (!left.empty())
        left.front() += impulse;
    inputGain_.process(left);
    inputFilter_.process(left);
    diffuserOne_.process(left);
    diffuserTwo_.process(left);
    tankOne_.process(left);
    tankTwo_.process(left);

    std::ranges::copy(left, right.begin());
    leftTap_.process(left);
    rightTap_.process(right);
    std::fill(outputLeft.begin() + static_cast<std::ptrdiff_t>(count), outputLeft.end(), 0.0F);
    std::fill(outputRight.begin() + static_cast<std::ptrdiff_t>(count), outputRight.end(), 0.0F);
}

} // namespace reverb::dsp
