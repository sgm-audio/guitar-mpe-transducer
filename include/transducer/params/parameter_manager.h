#pragma once

#include <clap/clap.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace transducer::params {

constexpr clap_id kParamBendRange = 0;
constexpr clap_id kParamTransientSensitivity = 1;
constexpr clap_id kParamGateThreshold = 2;
constexpr clap_id kParamRoutingMode = 3;
constexpr uint32_t kNumParameters = 4;

struct AudioParamsSnapshot {
    float bend_range{48.0f};
    float transient_sensitivity{0.012f};
    float gate_threshold{0.002f};
    uint32_t routing_mode{0}; // 0 = Direct, 1 = Target Highest, 2 = Target All
};

class ParameterManager {
public:
    // One atomic per parameter: parameters are independent, so no cross-field consistency is
    // needed. Wait-free for any number of writers (audio-thread events, main-thread state load)
    // and readers, with no torn or default-substituted reads.
    ParameterManager() = default;

    uint32_t get_param_count() const { return 4; }

    bool get_info(uint32_t index, clap_param_info_t* info) const {
        if (!info) return false;
        std::memset(info, 0, sizeof(*info));

        switch (index) {
        case 0:
            info->id = kParamBendRange;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
            std::strncpy(info->name, "Pitch Bend Range", sizeof(info->name) - 1);
            std::strncpy(info->module, "Routing", sizeof(info->module) - 1);
            info->min_value = 1.0;
            info->max_value = 96.0;
            info->default_value = 48.0;
            return true;
        case 1:
            info->id = kParamTransientSensitivity;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE;
            std::strncpy(info->name, "Transient Sensitivity", sizeof(info->name) - 1);
            std::strncpy(info->module, "Detector", sizeof(info->module) - 1);
            info->min_value = 0.001;
            info->max_value = 0.05;
            info->default_value = 0.012;
            return true;
        case 2:
            info->id = kParamGateThreshold;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE;
            std::strncpy(info->name, "Gate Threshold", sizeof(info->name) - 1);
            std::strncpy(info->module, "Detector", sizeof(info->module) - 1);
            info->min_value = 0.0005;
            info->max_value = 0.02;
            info->default_value = 0.002;
            return true;
        case 3:
            info->id = kParamRoutingMode;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
            std::strncpy(info->name, "Dual-Neck Routing Mode", sizeof(info->name) - 1);
            std::strncpy(info->module, "Routing", sizeof(info->module) - 1);
            info->min_value = 0.0;
            info->max_value = 2.0;
            info->default_value = 0.0;
            return true;
        default:
            return false;
        }
    }

    double get_value(clap_id param_id) const {
        AudioParamsSnapshot snap = get_audio_snapshot();
        switch (param_id) {
        case kParamBendRange: return snap.bend_range;
        case kParamTransientSensitivity: return snap.transient_sensitivity;
        case kParamGateThreshold: return snap.gate_threshold;
        case kParamRoutingMode: return static_cast<double>(snap.routing_mode);
        default: return 0.0;
        }
    }

    void set_value(clap_id param_id, double value) {
        switch (param_id) {
        case kParamBendRange:
            bend_range_.store(static_cast<float>(std::clamp(value, 1.0, 96.0)), std::memory_order_relaxed);
            break;
        case kParamTransientSensitivity:
            transient_sensitivity_.store(static_cast<float>(std::clamp(value, 0.001, 0.05)), std::memory_order_relaxed);
            break;
        case kParamGateThreshold:
            gate_threshold_.store(static_cast<float>(std::clamp(value, 0.0005, 0.02)), std::memory_order_relaxed);
            break;
        case kParamRoutingMode:
            routing_mode_.store(static_cast<uint32_t>(std::clamp(std::round(value), 0.0, 2.0)), std::memory_order_relaxed);
            break;
        default:
            break;
        }
    }

    bool value_to_text(clap_id param_id, double value, char* out_buf, uint32_t out_buf_size) const {
        if (!out_buf || out_buf_size == 0) return false;
        switch (param_id) {
        case kParamBendRange:
            std::snprintf(out_buf, out_buf_size, "%.1f st", value);
            return true;
        case kParamTransientSensitivity:
            std::snprintf(out_buf, out_buf_size, "%.4f", value);
            return true;
        case kParamGateThreshold:
            std::snprintf(out_buf, out_buf_size, "%.4f", value);
            return true;
        case kParamRoutingMode: {
            int mode = static_cast<int>(std::round(value));
            if (mode == 0) std::strncpy(out_buf, "Direct", out_buf_size - 1);
            else if (mode == 1) std::strncpy(out_buf, "Target Highest", out_buf_size - 1);
            else std::strncpy(out_buf, "Target All", out_buf_size - 1);
            return true;
        }
        default:
            return false;
        }
    }

    AudioParamsSnapshot get_audio_snapshot() const {
        AudioParamsSnapshot snap{};
        snap.bend_range = bend_range_.load(std::memory_order_relaxed);
        snap.transient_sensitivity = transient_sensitivity_.load(std::memory_order_relaxed);
        snap.gate_threshold = gate_threshold_.load(std::memory_order_relaxed);
        snap.routing_mode = routing_mode_.load(std::memory_order_relaxed);
        return snap;
    }

private:
    std::atomic<float> bend_range_{AudioParamsSnapshot{}.bend_range};
    std::atomic<float> transient_sensitivity_{AudioParamsSnapshot{}.transient_sensitivity};
    std::atomic<float> gate_threshold_{AudioParamsSnapshot{}.gate_threshold};
    std::atomic<uint32_t> routing_mode_{AudioParamsSnapshot{}.routing_mode};
};

} // namespace transducer::params
