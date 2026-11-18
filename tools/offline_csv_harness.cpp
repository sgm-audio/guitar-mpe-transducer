#include <transducer/dsp/string_pipeline.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr float kTwoPi = 6.28318530718f;

struct WavData {
    uint32_t sample_rate{48000};
    uint16_t channels{1};
    std::vector<std::vector<float>> audio; // [ch][sample]
};

bool read_u16(const uint8_t* p, uint16_t& out) {
    out = static_cast<uint16_t>(p[0] | (p[1] << 8));
    return true;
}

bool read_u32(const uint8_t* p, uint32_t& out) {
    out = static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
    return true;
}

bool load_wav(const std::string& path, WavData& out, std::string& err) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { err = "cannot open file: " + path; return false; }
    const size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> raw(sz);
    if (!f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz))) {
        err = "failed reading file";
        return false;
    }
    if (sz < 44 || std::memcmp(raw.data(), "RIFF", 4) != 0 || std::memcmp(raw.data() + 8, "WAVE", 4) != 0) {
        err = "not a RIFF/WAVE file";
        return false;
    }

    uint16_t format = 0, bits = 0;
    size_t data_off = 0, data_len = 0;
    size_t pos = 12;
    while (pos + 8 <= sz) {
        uint32_t csize = 0;
        read_u32(raw.data() + pos + 4, csize);
        if (std::memcmp(raw.data() + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= sz) {
            read_u16(raw.data() + pos + 8, format);
            read_u16(raw.data() + pos + 10, out.channels);
            read_u32(raw.data() + pos + 12, out.sample_rate);
            read_u16(raw.data() + pos + 22, bits);
        } else if (std::memcmp(raw.data() + pos, "data", 4) == 0) {
            data_off = pos + 8;
            data_len = std::min<size_t>(csize, sz - data_off);
        }
        pos += 8 + csize + (csize & 1u);
    }

    if (data_off == 0 || out.channels == 0 || out.channels > 6) {
        err = "unsupported channel count (must be 1..6) or missing data chunk";
        return false;
    }
    if (!((format == 1 && (bits == 16 || bits == 24)) || (format == 3 && bits == 32))) {
        err = "unsupported WAV format (need PCM 16/24-bit or IEEE float 32-bit)";
        return false;
    }

    const size_t bytes_per_sample = bits / 8;
    const size_t frame_bytes = bytes_per_sample * out.channels;
    const size_t num_frames = data_len / frame_bytes;
    out.audio.assign(out.channels, std::vector<float>(num_frames, 0.0f));

    const uint8_t* p = raw.data() + data_off;
    for (size_t i = 0; i < num_frames; ++i) {
        for (uint16_t c = 0; c < out.channels; ++c) {
            const uint8_t* s = p + (i * out.channels + c) * bytes_per_sample;
            float v = 0.0f;
            if (format == 1 && bits == 16) {
                int16_t x = static_cast<int16_t>(s[0] | (s[1] << 8));
                v = static_cast<float>(x) / 32768.0f;
            } else if (format == 1 && bits == 24) {
                int32_t x = static_cast<int32_t>((s[0] << 8) | (s[1] << 16) | (s[2] << 24)) >> 8;
                v = static_cast<float>(x) / 8388608.0f;
            } else {
                std::memcpy(&v, s, 4);
            }
            out.audio[c][i] = v;
        }
    }
    return true;
}

void add_pluck(std::vector<float>& buf, float sr, size_t start, float f0, float amp, float tau) {
    for (size_t i = start; i < buf.size(); ++i) {
        const float t = static_cast<float>(i - start) / sr;
        const float att = std::min(t / 0.001f, 1.0f);
        const float ph = kTwoPi * f0 * t;
        buf[i] += amp * att * std::exp(-t / tau) *
                  (std::sin(ph) + 0.4f * std::sin(2.0f * ph) + 0.2f * std::sin(3.0f * ph)) / 1.6f;
    }
}

WavData generate_synth(const std::string& mode) {
    WavData w;
    w.sample_rate = 48000;
    const float sr = 48000.0f;

    if (mode == "hex") {
        w.channels = 6;
        w.audio.assign(6, std::vector<float>(48000, 0.0f));
        const std::array<float, 6> open_hz{82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f};
        for (size_t s = 0; s < 6; ++s) {
            add_pluck(w.audio[s], sr, 1000 + s * 1440, open_hz[s], 0.4f, 0.5f);
        }
        return w;
    }

    w.channels = 1;
    if (mode == "bend") {
        w.audio.assign(1, std::vector<float>(48000, 0.0f));
        double ph = 0.0;
        for (size_t i = 1000; i < 48000; ++i) {
            const float t = static_cast<float>(i - 1000) / sr;
            const float bend = (t > 0.25f) ? std::min(2.0f, (t - 0.25f) / 0.20f * 2.0f) : 0.0f;
            const float vib = (t > 0.55f) ? 0.35f * std::sin(kTwoPi * 5.5f * (t - 0.55f)) : 0.0f;
            const float f = 220.0f * std::pow(2.0f, (bend + vib) / 12.0f);
            const float att = std::min(t / 0.001f, 1.0f);
            w.audio[0][i] = 0.45f * att * std::exp(-t / 1.2f) * static_cast<float>(std::sin(ph));
            ph += kTwoPi * f / sr;
        }
        return w;
    }

    // Default "pluck": E2, A3, E4
    w.audio.assign(1, std::vector<float>(72000, 0.0f));
    add_pluck(w.audio[0], sr, 1000, 82.41f, 0.4f, 0.35f);
    add_pluck(w.audio[0], sr, 24000, 220.0f, 0.45f, 0.35f);
    add_pluck(w.audio[0], sr, 48000, 329.63f, 0.4f, 0.35f);
    return w;
}

const char* event_name(transducer::dsp::CoreEvent::Type t) {
    using T = transducer::dsp::CoreEvent::Type;
    switch (t) {
    case T::kNoteOn: return "note_on";
    case T::kNoteOff: return "note_off";
    case T::kBend: return "bend";
    case T::kPressure: return "pressure";
    case T::kBrightness: return "brightness";
    }
    return "unknown";
}

} // namespace

int main(int argc, char** argv) {
    std::string wav_path;
    std::string synth_mode = "pluck";
    std::string out_path;
    float sensitivity = 0.012f;
    float gate = 0.002f;
    bool allow_chord_guard = true;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--wav" && i + 1 < argc) { wav_path = argv[++i]; }
        else if (a == "--synth" && i + 1 < argc) { synth_mode = argv[++i]; }
        else if (a == "--out" && i + 1 < argc) { out_path = argv[++i]; }
        else if (a == "--sensitivity" && i + 1 < argc) { sensitivity = std::stof(argv[++i]); }
        else if (a == "--gate" && i + 1 < argc) { gate = std::stof(argv[++i]); }
        else if (a == "--no-chord-guard") { allow_chord_guard = false; }
        else if (a == "-h" || a == "--help") {
            std::cout << "Usage: transducer_offline_harness [--wav file.wav | --synth pluck|bend|hex]\n"
                      << "       [--out events.csv] [--sensitivity 0.012] [--gate 0.002] [--no-chord-guard]\n";
            return 0;
        }
    }

    WavData wav;
    if (!wav_path.empty()) {
        std::string err;
        if (!load_wav(wav_path, wav, err)) {
            std::cerr << "Error: " << err << "\n";
            return 1;
        }
    } else {
        wav = generate_synth(synth_mode);
    }

    std::ofstream file_out;
    std::ostream* os = &std::cout;
    if (!out_path.empty()) {
        file_out.open(out_path);
        if (!file_out) {
            std::cerr << "Error: cannot open output CSV: " << out_path << "\n";
            return 1;
        }
        os = &file_out;
    }

    const size_t nch = wav.audio.size();
    const size_t nframes = wav.audio[0].size();
    const float sr = static_cast<float>(wav.sample_rate);

    std::array<transducer::dsp::StringPipeline, 6> pipelines{};
    transducer::dsp::StringPipeline::Config cfg;
    cfg.transient_threshold = sensitivity;
    cfg.gate_threshold = gate;
    cfg.chord_guard = allow_chord_guard && (nch == 1);

    for (size_t c = 0; c < nch; ++c) {
        pipelines[c].prepare(sr, static_cast<int>(c));
        pipelines[c].set_config(cfg);
    }

    *os << "sample,time_ms,string,type,note_id,key,value\n";
    *os << std::fixed << std::setprecision(4);

    for (size_t i = 0; i < nframes; ++i) {
        const double time_ms = static_cast<double>(i) * 1000.0 / static_cast<double>(sr);
        for (size_t c = 0; c < nch; ++c) {
            pipelines[c].process_sample(wav.audio[c][i], static_cast<uint32_t>(i),
                [&](const transducer::dsp::CoreEvent& e) {
                    if (e.type == transducer::dsp::CoreEvent::Type::kBrightness) return;
                    *os << i << "," << time_ms << "," << (e.string_index + 1) << ","
                        << event_name(e.type) << "," << e.note_id << "," << e.key << ","
                        << e.value << "\n";
                });
        }
    }

    return 0;
}
