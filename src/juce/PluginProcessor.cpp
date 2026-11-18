#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kParamBendRange = "bend_range";
constexpr const char* kParamTransientSens = "transient_sensitivity";
constexpr const char* kParamGateThreshold = "gate_threshold";
constexpr const char* kParamRoutingMode = "routing_mode";
constexpr const char* kParamOutputDialect = "output_dialect";
constexpr const char* kStateMagic = "GTRJ";
} // namespace

GuitarTransducerAudioProcessor::GuitarTransducerAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Guitar In", juce::AudioChannelSet::discreteChannels(6), true)
                         .withOutput("Guitar Thru", juce::AudioChannelSet::discreteChannels(6), true)),
      apvts_(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bend_range_param_ = apvts_.getRawParameterValue(kParamBendRange);
    transient_sens_param_ = apvts_.getRawParameterValue(kParamTransientSens);
    gate_thresh_param_ = apvts_.getRawParameterValue(kParamGateThreshold);
    routing_mode_param_ = apvts_.getRawParameterValue(kParamRoutingMode);
    output_dialect_param_ = apvts_.getRawParameterValue(kParamOutputDialect);

    string_midi_ch_.fill(2);
    string_note_.fill(-1);
    string_cents_.fill(0.0f);
    string_pressure_.fill(0.0f);
    rpn_sent_range_.fill(-1.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GuitarTransducerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamBendRange, 1},
        "Pitch Bend Range",
        juce::NormalisableRange<float>(1.0f, 96.0f, 0.5f),
        48.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamTransientSens, 1},
        "Transient Sensitivity",
        juce::NormalisableRange<float>(0.001f, 0.05f, 0.0005f),
        0.012f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamGateThreshold, 1},
        "Gate Threshold",
        juce::NormalisableRange<float>(0.0005f, 0.02f, 0.0001f),
        0.002f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{kParamRoutingMode, 1},
        "Dual-Neck Routing Mode",
        juce::StringArray{"Direct", "Target Highest", "Target All"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{kParamOutputDialect, 1},
        "Output Dialect",
        juce::StringArray{"MPE (Per-Channel Bend)", "Single-Channel MIDI"},
        0));

    return {params.begin(), params.end()};
}

bool GuitarTransducerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    return in.size() >= 1 && in.size() <= static_cast<int>(kMaxStrings) && in == out;
}

void GuitarTransducerAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    const float sr = static_cast<float>(sampleRate);
    for (size_t i = 0; i < kMaxStrings; ++i) {
        strings_[i].prepare(sr, static_cast<int>(i));
        string_midi_ch_[i] = static_cast<int>(i) + 2; // MPE member channels 2..7
        string_note_[i] = -1;
        string_cents_[i] = 0.0f;
        string_pressure_[i] = 0.0f;
    }
    rpn_sent_range_.fill(-1.0f);
    next_mono_mpe_ch_ = 2;
    voice_allocator_.reset();
    scratch_midi_.ensureSize(4096);

    // Audio pass-through has 0 samples of lookahead delay; MIDI onset resolution takes ~5 ms peak-hold.
    setLatencySamples(0);
}

void GuitarTransducerAudioProcessor::releaseResources()
{
    for (auto& s : strings_) s.reset();
    string_note_.fill(-1);
    string_cents_.fill(0.0f);
    string_pressure_.fill(0.0f);
    telemetry_.last_active_string.store(-1, std::memory_order_relaxed);
    telemetry_.midi_note.store(-1, std::memory_order_relaxed);
    telemetry_.cents_offset.store(0.0f, std::memory_order_relaxed);
    telemetry_.pressure.store(0.0f, std::memory_order_relaxed);
    telemetry_.string_mask.store(0, std::memory_order_relaxed);
    voice_allocator_.reset();
}

void GuitarTransducerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals no_denormals;

    const int num_samples = buffer.getNumSamples();
    const int num_ch = std::min(buffer.getNumChannels(), static_cast<int>(kMaxStrings));
    if (num_samples == 0 || num_ch == 0) return;

    const float bend_range = bend_range_param_->load(std::memory_order_relaxed);
    const bool mpe_mode = (output_dialect_param_->load(std::memory_order_relaxed) < 0.5f);

    transducer::dsp::StringPipeline::Config cfg;
    cfg.transient_threshold = transient_sens_param_->load(std::memory_order_relaxed);
    cfg.gate_threshold = gate_thresh_param_->load(std::memory_order_relaxed);
    cfg.chord_guard = (num_ch <= 1);
    for (int c = 0; c < num_ch; ++c) {
        strings_[static_cast<size_t>(c)].set_config(cfg);
    }

    const int routing = static_cast<int>(std::round(routing_mode_param_->load(std::memory_order_relaxed)));
    voice_allocator_.set_routing_mode(static_cast<transducer::voice::RoutingMode>(std::clamp(routing, 0, 2)));

    scratch_midi_.clear();

    // Forward incoming keyboard MIDI and feed VoiceAllocator
    for (const auto meta : midiMessages) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) {
            voice_allocator_.onKeyNoteOn(msg.getNoteNumber(), static_cast<int16_t>(msg.getNoteNumber()),
                                         msg.getFloatVelocity(), static_cast<int16_t>(msg.getChannel() - 1));
        } else if (msg.isNoteOff()) {
            voice_allocator_.onKeyNoteOff(msg.getNoteNumber(), static_cast<int16_t>(msg.getNoteNumber()),
                                          static_cast<int16_t>(msg.getChannel() - 1));
        }
        scratch_midi_.addEvent(msg, meta.samplePosition);
    }

    auto emit_core = [&](const transducer::dsp::CoreEvent& e) {
        using T = transducer::dsp::CoreEvent::Type;
        const size_t si = static_cast<size_t>(std::clamp<int16_t>(e.string_index, 0, kMaxStrings - 1));

        if (e.type == T::kNoteOn) {
            if (!mpe_mode) {
                string_midi_ch_[si] = 1;
            } else if (num_ch <= 1) {
                string_midi_ch_[si] = next_mono_mpe_ch_;
                next_mono_mpe_ch_ = (next_mono_mpe_ch_ >= 16) ? 2 : (next_mono_mpe_ch_ + 1);
            } else {
                string_midi_ch_[si] = static_cast<int>(si) + 2;
            }
        }

        const int ch = string_midi_ch_[si];
        const int pos = static_cast<int>(e.offset);
        const int note = std::clamp<int>(e.key, 0, 127);

        switch (e.type) {
        case T::kNoteOn: {
            string_note_[si] = note;
            string_cents_[si] = 0.0f;
            telemetry_.last_active_string.store(static_cast<int>(si), std::memory_order_relaxed);
            telemetry_.midi_note.store(string_note_[si], std::memory_order_relaxed);
            telemetry_.cents_offset.store(0.0f, std::memory_order_relaxed);
            telemetry_.pressure.store(string_pressure_[si], std::memory_order_relaxed);
            if (rpn_sent_range_[static_cast<size_t>(ch - 1)] != bend_range) {
                const int semi = std::clamp(static_cast<int>(std::round(bend_range)), 1, 96);
                scratch_midi_.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 0), pos);
                scratch_midi_.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 0), pos);
                scratch_midi_.addEvent(juce::MidiMessage::controllerEvent(ch, 6, semi), pos);
                scratch_midi_.addEvent(juce::MidiMessage::controllerEvent(ch, 38, 0), pos);
                rpn_sent_range_[static_cast<size_t>(ch - 1)] = bend_range;
            }
            scratch_midi_.addEvent(juce::MidiMessage::noteOn(ch, note, std::clamp(e.value, 0.01f, 1.0f)), pos);
            break;
        }
        case T::kNoteOff:
            string_note_[si] = -1;
            string_cents_[si] = 0.0f;
            string_pressure_[si] = 0.0f;
            telemetry_.last_active_string.store(static_cast<int>(si), std::memory_order_relaxed);
            telemetry_.midi_note.store(-1, std::memory_order_relaxed);
            telemetry_.cents_offset.store(0.0f, std::memory_order_relaxed);
            telemetry_.pressure.store(0.0f, std::memory_order_relaxed);
            scratch_midi_.addEvent(juce::MidiMessage::noteOff(ch, note, std::clamp(e.value, 0.0f, 1.0f)), pos);
            break;
        case T::kBend: {
            string_cents_[si] = e.value * 100.0f;
            telemetry_.last_active_string.store(static_cast<int>(si), std::memory_order_relaxed);
            telemetry_.midi_note.store(string_note_[si], std::memory_order_relaxed);
            telemetry_.cents_offset.store(string_cents_[si], std::memory_order_relaxed);
            telemetry_.pressure.store(string_pressure_[si], std::memory_order_relaxed);
            const float norm = std::clamp(e.value / bend_range, -1.0f, 1.0f);
            const int wheel = std::clamp(static_cast<int>(std::round((norm * 0.5f + 0.5f) * 16383.0f)), 0, 16383);
            scratch_midi_.addEvent(juce::MidiMessage::pitchWheel(ch, wheel), pos);
            break;
        }
        case T::kPressure: {
            string_pressure_[si] = std::clamp(e.value, 0.0f, 1.0f);
            telemetry_.last_active_string.store(static_cast<int>(si), std::memory_order_relaxed);
            telemetry_.midi_note.store(string_note_[si], std::memory_order_relaxed);
            telemetry_.cents_offset.store(string_cents_[si], std::memory_order_relaxed);
            telemetry_.pressure.store(string_pressure_[si], std::memory_order_relaxed);
            const int press = std::clamp(static_cast<int>(std::round(e.value * 127.0f)), 0, 127);
            scratch_midi_.addEvent(juce::MidiMessage::channelPressureChange(ch, press), pos);
            break;
        }
        case T::kBrightness: {
            const int cc74 = std::clamp(static_cast<int>(std::round(e.value * 127.0f)), 0, 127);
            scratch_midi_.addEvent(juce::MidiMessage::controllerEvent(ch, 74, cc74), pos);
            break;
        }
        }
    };

    for (int s = 0; s < num_samples; ++s) {
        for (int c = 0; c < num_ch; ++c) {
            const float sample = buffer.getSample(c, s);
            strings_[static_cast<size_t>(c)].process_sample(sample, static_cast<uint32_t>(s), emit_core);
        }
    }

    uint8_t mask = 0;
    for (int c = 0; c < num_ch; ++c) {
        if (strings_[static_cast<size_t>(c)].note_active()) {
            mask |= static_cast<uint8_t>(1u << c);
        }
    }
    if (mask == 0) {
        telemetry_.last_active_string.store(-1, std::memory_order_relaxed);
        telemetry_.midi_note.store(-1, std::memory_order_relaxed);
        telemetry_.cents_offset.store(0.0f, std::memory_order_relaxed);
        telemetry_.pressure.store(0.0f, std::memory_order_relaxed);
    }
    telemetry_.string_mask.store(mask, std::memory_order_relaxed);
    telemetry_.active_channels.store(static_cast<uint8_t>(num_ch), std::memory_order_relaxed);

    midiMessages.swapWith(scratch_midi_);
}

void GuitarTransducerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty("magic", kStateMagic, nullptr);
    state.setProperty("version", kStateVersion, nullptr);
    if (auto xml = state.createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

void GuitarTransducerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!tree.isValid() || tree.getType() != apvts_.state.getType()) return;
        if (tree.getProperty("magic").toString() != kStateMagic) return;
        const int ver = static_cast<int>(tree.getProperty("version", 0));
        if (ver < 1 || ver > kStateVersion) return;
        apvts_.replaceState(tree);
    }
}

juce::AudioProcessorEditor* GuitarTransducerAudioProcessor::createEditor()
{
    return new GuitarTransducerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarTransducerAudioProcessor();
}
