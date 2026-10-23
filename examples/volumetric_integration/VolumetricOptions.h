#pragma once

#include <charconv>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ku::volumetric {
// Example-local controls; no mutable uniform buffer is shared by GPU submissions.
struct Settings {
    float time = 0;
    float timeStep = 0;
    float density = 1;
    float light = 1;
    float strongFog = 0;
    int steps = 100;
    int shadowSteps = 16;
    int integration = 0; // analytical, basic, transmittance first, split comparison
    bool paused = false;
    bool noise = true;
    bool shadows = true;
};
struct Options {
    Settings settings;
    std::string readbackPrefix;
    std::vector<std::string> forwarded;
};
inline Options parseOptions(std::span<const std::string_view> args)
{
    Options result;
    for (size_t i = 0; i < args.size(); ++i) {
        const auto key = args[i];
        const auto value = [&]() {
            if (++i == args.size()) throw std::invalid_argument(std::string(key) + " requires a value");
            return args[i];
        };
        const auto number = [&](float low, float high) {
            const auto text = value();
            float parsed = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(parsed) || parsed < low || parsed > high)
                throw std::invalid_argument(std::string(key) + " is outside its allowed range");
            return parsed;
        };
        const auto integer = [&](int low, int high) {
            const auto text = value();
            int parsed = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if (error != std::errc{} || end != text.data() + text.size() || parsed < low || parsed > high)
                throw std::invalid_argument(std::string(key) + " is outside its allowed integer range");
            return parsed;
        };
        auto& s = result.settings;
        if (key == "--volumetric-time") { s.time = number(0, 1000000); s.paused = true; }
        else if (key == "--volumetric-time-step") s.timeStep = number(0, 100);
        else if (key == "--volumetric-density") s.density = number(0, 20);
        else if (key == "--volumetric-light") s.light = number(0, 10);
        else if (key == "--volumetric-strong-fog") s.strongFog = number(0, 10);
        else if (key == "--volumetric-steps") s.steps = integer(16, 256);
        else if (key == "--volumetric-shadow-steps") s.shadowSteps = integer(1, 64);
        else if (key == "--volumetric-integration") {
            const auto mode = value();
            if (mode == "improved") s.integration = 0;
            else if (mode == "basic") s.integration = 1;
            else if (mode == "transmittance-first") s.integration = 2;
            else if (mode == "split") s.integration = 3;
            else throw std::invalid_argument("Unknown volumetric integration mode");
        } else if (key == "--volumetric-no-noise") s.noise = false;
        else if (key == "--volumetric-no-shadows") s.shadows = false;
        else if (key == "--volumetric-readback-prefix") {
            result.readbackPrefix = value();
            if (result.readbackPrefix.empty()) throw std::invalid_argument("Readback prefix must not be empty");
        } else if (key.starts_with("--volumetric-")) throw std::invalid_argument("Unknown option: " + std::string(key));
        else result.forwarded.emplace_back(key);
    }
    if (result.settings.timeStep > 0 && !result.settings.paused)
        throw std::invalid_argument("--volumetric-time-step requires --volumetric-time");
    return result;
}
} // namespace ku::volumetric
