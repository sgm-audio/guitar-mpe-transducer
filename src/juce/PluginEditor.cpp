#include "PluginEditor.h"
#include <transducer/presets/factory_presets.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kSensMin = 0.001f;
constexpr float kSensMax = 0.05f;
constexpr float kGateMin = 0.0005f;
constexpr float kGateMax = 0.02f;

// Declarative JIVE layout: all Flex spacing, padding, tab strip sizing, and
// box-model proportions live here and in kEditorStyleJson (zero imperative layout math).
constexpr const char* kEditorXml = R"xml(
<VolcanicRoot id="root" width="1200" height="675" flex-direction="column" padding="54 74 60 74" gap="10">
    <Component id="header" height="38" flex-shrink="0" flex-direction="row" justify-content="space-between" align-items="center" gap="14">
        <GlowingTitle id="app-title" flex-grow="1" height="38" />
        <Component id="header-right" flex-direction="row" align-items="center" gap="14" height="36">
            <ComboBox id="preset_selector" width="256" height="33" />
            <Text id="format-badge" width="168" height="22">CLAP / MPE / MIDI 2.0</Text>
        </Component>
    </Component>

    <Component id="telemetry-section" height="88" flex-shrink="0" flex-direction="column" gap="8">
        <Component id="telemetry-row" height="55" flex-direction="row" align-items="center" gap="20">
            <PitchReadout id="pitch-readout" width="194" height="55" />
            <BendMeter id="bend-meter" flex-grow="1" height="46" />
            <PressureMeter id="pressure-meter" width="310" height="46" />
        </Component>

        <Component id="string-tabs" height="25" flex-direction="row" gap="6">
            <StringTab id="string-tab-1" class="string-tab" flex-grow="1" height="25" />
            <StringTab id="string-tab-2" class="string-tab" flex-grow="1" height="25" />
            <StringTab id="string-tab-3" class="string-tab" flex-grow="1" height="25" />
            <StringTab id="string-tab-4" class="string-tab" flex-grow="1" height="25" />
            <StringTab id="string-tab-5" class="string-tab" flex-grow="1" height="25" />
            <StringTab id="string-tab-6" class="string-tab" flex-grow="1" height="25" />
        </Component>
    </Component>

    <DetectionPanel id="detection-panel" flex-grow="1" flex-direction="column">
        <DetectionHeader id="detection-header" height="30" flex-shrink="0" flex-direction="row" align-items="center" padding="0 16 0 16">
            <Text id="detection-title" class="section-header" flex-grow="1">DETECTION &amp; TRACKING</Text>
        </DetectionHeader>

        <DetectionBay id="detection-bay" flex-grow="1" flex-direction="row" padding="8 18 8 18" gap="14">
            <Component class="knob-column" flex-grow="1" flex-direction="column" align-items="center" gap="5">
                <Text class="knob-label" width="100%" height="21">SENSITIVITY</Text>
                <Component class="knob-slot" flex-grow="1" width="100%" align-items="center" justify-content="center">
                    <Knob id="transient_sensitivity" width="100%" height="100%" />
                </Component>
            </Component>

            <Component class="knob-column" flex-grow="1" flex-direction="column" align-items="center" gap="5">
                <Text class="knob-label" width="100%" height="21">GATE</Text>
                <Component class="knob-slot" flex-grow="1" width="100%" align-items="center" justify-content="center">
                    <Knob id="gate_threshold" width="100%" height="100%" />
                </Component>
            </Component>

            <Component class="knob-column" flex-grow="1" flex-direction="column" align-items="center" gap="5">
                <Text class="knob-label" width="100%" height="21">BEND RANGE</Text>
                <Component class="knob-slot" flex-grow="1" width="100%" align-items="center" justify-content="center">
                    <Knob id="bend_range" width="100%" height="100%" />
                </Component>
            </Component>
        </DetectionBay>
    </DetectionPanel>

    <RoutingPanel id="routing-panel" height="86" flex-shrink="0" flex-direction="column" padding="8 16 11 16" gap="6">
        <Text id="routing-title" class="section-subtitle" height="19">ROUTING &amp; OUTPUT</Text>

        <Component id="routing-controls" flex-grow="1" flex-direction="row" align-items="center" gap="24">
            <Component class="routing-field" flex-grow="1" flex-direction="row" align-items="center" gap="8">
                <Text class="field-label" width="76" height="22">ROUTING:</Text>
                <ComboBox id="routing_mode" flex-grow="1" height="34">
                    <Option>Direct (Mono)</Option>
                    <Option>Target Highest (Poly)</Option>
                    <Option>Target All (Poly)</Option>
                </ComboBox>
            </Component>

            <Component class="routing-field" flex-grow="1" flex-direction="row" align-items="center" gap="8">
                <Text class="field-label" width="68" height="22">OUTPUT:</Text>
                <ComboBox id="output_dialect" flex-grow="1" height="34">
                    <Option>CLAP Notes / MPE</Option>
                    <Option>MIDI 2.0 UMP / Single-Ch</Option>
                </ComboBox>
            </Component>
        </Component>
    </RoutingPanel>
</VolcanicRoot>
)xml";

constexpr const char* kEditorStyleJson = R"json({
    "$cream": "#F5E6D3",
    "$bronze-text": "#C4A882",
    "$combo-text": "#F4D2A4",
    "$amber": "#FF9A3C",
    "$molten": "#FF6B1A",
    "$bronze-border": "#6B4A2F",
    "$groove": "#0A0806",
    "$popup-bg": "#16110D",

    "background": "rgba(0, 0, 0, 0)",
    "foreground": "$cream",
    "font-family": "Liberation Sans Narrow",
    "font-size": 15,

    "#format-badge": {
        "foreground": "$bronze-text",
        "font-size": 14.5,
        "text-align": "centred-right"
    },

    ".section-header": {
        "foreground": "$cream",
        "font-size": 15.5,
        "font-styles": "bold",
        "text-align": "centred-left"
    },

    ".section-subtitle": {
        "foreground": "$bronze-text",
        "font-size": 15,
        "font-styles": "bold",
        "text-align": "centred-left"
    },

    ".knob-label": {
        "foreground": "$bronze-text",
        "font-size": 15,
        "text-align": "centred"
    },

    ".field-label": {
        "foreground": "$bronze-text",
        "font-size": 15,
        "text-align": "centred-left"
    },

    "ComboBox": {
        "background": "$groove",
        "foreground": "$combo-text",
        "border": "$bronze-border",
        "border-width": 2,
        "border-radius": 4,
        "font-family": "Liberation Sans",
        "font-size": 15.5
    },

    "ComboBox Popup": {
        "background": "$popup-bg",
        "border": "$bronze-border",
        "border-width": 1,
        "border-radius": 4,
        "foreground": "$cream"
    },

    "ComboBox Option": {
        "foreground": "$cream",
        ":hover": {
            "background": "rgba(255, 107, 26, 0.28)",
            "foreground": "#FFD080"
        }
    }
})json";

juce::Component* findComponentById(juce::Component& root, const juce::String& targetId)
{
    if (root.getComponentID() == targetId)
        return &root;

    for (auto* child : root.getChildren()) {
        if (child != nullptr) {
            if (auto* found = findComponentById(*child, targetId))
                return found;
        }
    }
    return nullptr;
}
} // namespace

juce::ValueTree GuitarTransducerAudioProcessorEditor::createLayoutTree()
{
    auto tree = jive::parseXML(kEditorXml);
    if (!tree.isValid())
        tree = juce::ValueTree("VolcanicRoot");
    tree.setProperty("style", jive::parseJSON(kEditorStyleJson), nullptr);
    return tree;
}

GuitarTransducerAudioProcessorEditor::GuitarTransducerAudioProcessorEditor(
    GuitarTransducerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      telemetry_snapshot_(transducer::ui::TelemetrySnapshot::load(p.telemetry())),
      layout_tree_(createLayoutTree())
{
    setLookAndFeel(&look_and_feel_);
    setOpaque(true);

    registerCustomComponents();
    interpreter_.autoAttachParameters(processor_.apvts());
    root_item_ = interpreter_.interpret(layout_tree_);

    if (root_item_ != nullptr) {
        if (auto* rootComp = root_item_->getComponent().get()) {
            // Attach JIVE-derived LookAndFeel BEFORE adding to parent so StyleSheet::create
            // immediately binds our JSON stylesheet to the LookAndFeel.
            rootComp->setLookAndFeel(&look_and_feel_);
            addAndMakeVisible(rootComp);
            rootComp->sendLookAndFeelChange();

            bindControlsAfterInterpret(*rootComp);
        }
    }

    // 9-slice volcanic bronze outer chassis overlay (non-interactive, always on top)
    addAndMakeVisible(chassis_overlay_);
    chassis_overlay_.toFront(false);

    startTimerHz(30);
    setSize(1200, 675);
}

GuitarTransducerAudioProcessorEditor::~GuitarTransducerAudioProcessorEditor()
{
    stopTimer();
    if (root_item_ != nullptr) {
        if (auto* rootComp = root_item_->getComponent().get())
            rootComp->setLookAndFeel(nullptr);
    }
    setLookAndFeel(nullptr);
}

void GuitarTransducerAudioProcessorEditor::registerCustomComponents()
{
    auto& factory = interpreter_.getComponentFactory();
    factory.set("VolcanicRoot", [] { return std::make_shared<transducer::ui::VolcanicRoot>(); });
    factory.set("GlowingTitle", [] { return std::make_shared<transducer::ui::GlowingTitle>(); });
    factory.set("DetectionPanel", [] { return std::make_shared<transducer::ui::DetectionPanel>(); });
    factory.set("DetectionHeader", [] { return std::make_shared<transducer::ui::DetectionHeader>(); });
    factory.set("DetectionBay", [] { return std::make_shared<transducer::ui::DetectionBay>(); });
    factory.set("RoutingPanel", [] { return std::make_shared<transducer::ui::RoutingPanel>(); });
    factory.set("PitchReadout", [this] {
        return std::make_shared<transducer::ui::PitchReadoutView>(telemetry_snapshot_);
    });
    factory.set("BendMeter", [this] {
        return std::make_shared<transducer::ui::TelemetryBarView>(
            telemetry_snapshot_, transducer::ui::TelemetryBarView::Mode::bend);
    });
    factory.set("PressureMeter", [this] {
        return std::make_shared<transducer::ui::TelemetryBarView>(
            telemetry_snapshot_, transducer::ui::TelemetryBarView::Mode::pressure);
    });
    factory.set("StringTab", [this] {
        return std::make_shared<transducer::ui::StringTabView>(telemetry_snapshot_);
    });
}

void GuitarTransducerAudioProcessorEditor::bindControlsAfterInterpret(juce::Component& rootComp)
{
    // Cache JIVE-managed telemetry view pointers for 30 Hz repaints
    pitch_view_ = findComponentById(rootComp, "pitch-readout");
    bend_view_ = findComponentById(rootComp, "bend-meter");
    pressure_view_ = findComponentById(rootComp, "pressure-meter");
    for (int i = 0; i < 6; ++i)
        string_tab_views_[static_cast<size_t>(i)] = findComponentById(rootComp, "string-tab-" + juce::String(i + 1));

    // Configure hero rotary knobs (already parameter-attached by JIVE autoAttachParameters)
    auto configureKnob = [&](const char* id,
                             const char* knobStyle,
                             std::function<juce::String(double)> toText,
                             std::function<double(const juce::String&)> fromText) {
        auto* slider = dynamic_cast<juce::Slider*>(findComponentById(rootComp, id));
        if (slider == nullptr)
            return;

        slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 24);
        slider->getProperties().set("knobStyle", knobStyle);
        slider->textFromValueFunction = std::move(toText);
        slider->valueFromTextFunction = std::move(fromText);
        slider->lookAndFeelChanged();
        slider->updateText();
    };

    configureKnob(
        "transient_sensitivity", "orbital",
        [](double v) {
            const double norm = std::clamp((v - kSensMin) / (kSensMax - kSensMin), 0.0, 1.0);
            return juce::String(norm, 2);
        },
        [](const juce::String& text) {
            const double norm = std::clamp(text.getDoubleValue(), 0.0, 1.0);
            return kSensMin + norm * (kSensMax - kSensMin);
        });

    configureKnob(
        "gate_threshold", "orbital_dense",
        [](double v) {
            const double db = juce::Decibels::gainToDecibels(std::max(v, 1e-6), -96.0);
            return juce::String(db, 1) + " dB";
        },
        [](const juce::String& text) {
            const double db = text.retainCharacters("-0123456789.").getDoubleValue();
            return std::clamp(static_cast<double>(juce::Decibels::decibelsToGain(db)),
                              static_cast<double>(kGateMin), static_cast<double>(kGateMax));
        });

    configureKnob(
        "bend_range", "ticks",
        [](double v) { return juce::String(static_cast<int>(std::round(v))) + " st"; },
        [](const juce::String& text) {
            return std::clamp(text.retainCharacters("0123456789.").getDoubleValue(), 1.0, 96.0);
        });

    // Ensure choice ComboBoxes reflect exact APVTS choices and active selection immediately
    auto syncChoiceCombo = [&](const char* paramId) {
        auto* combo = dynamic_cast<juce::ComboBox*>(findComponentById(rootComp, paramId));
        auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor_.apvts().getParameter(paramId));
        if (combo == nullptr || choice == nullptr)
            return;

        if (combo->getNumItems() != choice->choices.size()) {
            combo->clear(juce::dontSendNotification);
            combo->addItemList(choice->choices, 1);
        }
        combo->setSelectedItemIndex(choice->getIndex(), juce::dontSendNotification);
        combo->lookAndFeelChanged();
    };
    syncChoiceCombo("routing_mode");
    syncChoiceCombo("output_dialect");

    // Factory preset selector -> GTRJ v1 state serialization
    preset_combo_ = dynamic_cast<juce::ComboBox*>(findComponentById(rootComp, "preset_selector"));
    if (preset_combo_ != nullptr) {
        preset_combo_->clear(juce::dontSendNotification);
        for (size_t i = 0; i < transducer::presets::kNumFactoryPresets; ++i) {
            preset_combo_->addItem(transducer::presets::kFactoryPresets[i].name,
                                   static_cast<int>(i) + 1);
        }
        preset_combo_->setSelectedItemIndex(processor_.getCurrentProgram(), juce::dontSendNotification);
        preset_combo_->lookAndFeelChanged();
        preset_combo_->onChange = [this] {
            const int idx = preset_combo_->getSelectedItemIndex();
            if (idx < 0 || idx >= static_cast<int>(transducer::presets::kNumFactoryPresets))
                return;
            const auto& pr = transducer::presets::kFactoryPresets[static_cast<size_t>(idx)];

            juce::ValueTree state(processor_.apvts().state.getType());
            state.setProperty("magic", "GTRJ", nullptr);
            state.setProperty("version", GuitarTransducerAudioProcessor::kStateVersion, nullptr);
            auto addParam = [&](const char* id, float val) {
                juce::ValueTree pNode("PARAM");
                pNode.setProperty("id", id, nullptr);
                pNode.setProperty("value", val, nullptr);
                state.appendChild(pNode, nullptr);
            };
            addParam("bend_range", pr.bend_range);
            addParam("transient_sensitivity", pr.transient_sensitivity);
            addParam("gate_threshold", pr.gate_threshold);
            addParam("routing_mode", static_cast<float>(pr.routing_mode));
            addParam("output_dialect", static_cast<float>(pr.output_dialect));

            if (auto xml = state.createXml()) {
                juce::MemoryBlock block;
                juce::AudioProcessor::copyXmlToBinary(*xml, block);
                processor_.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
            }
        };
    }
}

void GuitarTransducerAudioProcessorEditor::resized()
{
    if (root_item_ != nullptr) {
        if (auto* rootComp = root_item_->getComponent().get())
            rootComp->setBounds(getLocalBounds());
    }
    chassis_overlay_.setBounds(getLocalBounds());
    chassis_overlay_.toFront(false);
}

void GuitarTransducerAudioProcessorEditor::refreshTelemetryForPreview()
{
    telemetry_snapshot_ = transducer::ui::TelemetrySnapshot::load(processor_.telemetry());
    if (pitch_view_ != nullptr) pitch_view_->repaint();
    if (bend_view_ != nullptr) bend_view_->repaint();
    if (pressure_view_ != nullptr) pressure_view_->repaint();
    for (auto* tab : string_tab_views_) {
        if (tab != nullptr)
            tab->repaint();
    }
}

void GuitarTransducerAudioProcessorEditor::timerCallback()
{
    const auto fresh = transducer::ui::TelemetrySnapshot::load(processor_.telemetry());
    if (fresh.differsFrom(telemetry_snapshot_)) {
        telemetry_snapshot_ = fresh;
        if (pitch_view_ != nullptr) pitch_view_->repaint();
        if (bend_view_ != nullptr) bend_view_->repaint();
        if (pressure_view_ != nullptr) pressure_view_->repaint();
        for (auto* tab : string_tab_views_) {
            if (tab != nullptr)
                tab->repaint();
        }
    }
}
