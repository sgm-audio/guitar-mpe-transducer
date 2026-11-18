#include "PluginEditor.h"

namespace {
constexpr const char* kEditorXml = R"xml(
<Component id="root" width="580" height="370" flex-direction="column" padding="16" gap="12">
    <Component id="header" height="28" flex-direction="row" justify-content="space-between" align-items="center">
        <Text font-size="15" font-weight="bold">GUITAR MPE TRANSDUCER</Text>
        <Text id="badge" font-size="11">CLAP / MPE / MIDI 2.0</Text>
    </Component>

    <Component id="telemetry-slot" height="106" flex-shrink="0" />

    <Component id="controls-row" flex-grow="1" flex-direction="row" gap="10">
        <Component class="card" flex-grow="1" flex-direction="column" padding="10" gap="6">
            <Text class="label">SENSITIVITY</Text>
            <Slider id="transient_sensitivity" style="rotary" text-box="below" flex-grow="1" />
        </Component>

        <Component class="card" flex-grow="1" flex-direction="column" padding="10" gap="6">
            <Text class="label">GATE</Text>
            <Slider id="gate_threshold" style="rotary" text-box="below" flex-grow="1" />
        </Component>

        <Component class="card" flex-grow="1" flex-direction="column" padding="10" gap="6">
            <Text class="label">BEND RANGE</Text>
            <Slider id="bend_range" style="rotary" text-box="below" flex-grow="1" />
        </Component>
    </Component>

    <Component id="modes-row" height="58" flex-direction="row" gap="10">
        <Component class="card" flex-grow="1" flex-direction="column" padding="8" gap="4">
            <Text class="label">TRACKING / ROUTING</Text>
            <ComboBox id="routing_mode" flex-grow="1">
                <Option>Direct (Mono)</Option>
                <Option>Target Highest (Poly)</Option>
                <Option>Target All (Poly)</Option>
            </ComboBox>
        </Component>

        <Component class="card" flex-grow="1" flex-direction="column" padding="8" gap="4">
            <Text class="label">OUTPUT DIALECT</Text>
            <ComboBox id="output_dialect" flex-grow="1">
                <Option>CLAP Notes / MPE</Option>
                <Option>MIDI 2.0 UMP / Single-Ch</Option>
            </ComboBox>
        </Component>
    </Component>
</Component>
)xml";

constexpr const char* kEditorStyleJson = R"json({
    "background": "#0D0F12",
    "foreground": "#E5E7EB",
    "font-family": "Inter, Helvetica Neue, Arial, sans-serif",
    "#badge": {
        "foreground": "#38BDF8"
    },
    ".card": {
        "background": "#14171C",
        "border": "#232832",
        "border-width": 1,
        "border-radius": 6
    },
    ".label": {
        "foreground": "#9CA3AF",
        "font-size": 10.5
    },
    "Slider": {
        "foreground": "#38BDF8",
        "background": "#0D0F12"
    },
    "ComboBox": {
        "background": "#0D0F12",
        "foreground": "#E5E7EB",
        "border": "#232832",
        "border-width": 1,
        "border-radius": 4
    }
})json";
} // namespace

juce::ValueTree GuitarTransducerAudioProcessorEditor::createLayoutTree()
{
    auto xml = juce::XmlDocument::parse(kEditorXml);
    auto tree = xml ? juce::ValueTree::fromXml(*xml) : juce::ValueTree("Component");
    tree.setProperty("style", kEditorStyleJson, nullptr);
    return tree;
}

GuitarTransducerAudioProcessorEditor::GuitarTransducerAudioProcessorEditor(
    GuitarTransducerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      layout_tree_(createLayoutTree()),
      telemetry_meter_(p.telemetry())
{
    setLookAndFeel(&look_and_feel_);
    interpreter_.autoAttachParameters(processor_.apvts());
    root_item_ = interpreter_.interpret(layout_tree_);

    if (root_item_ != nullptr) {
        if (auto* root_comp = root_item_->getComponent()) {
            root_comp->setLookAndFeel(&look_and_feel_);
            addAndMakeVisible(root_comp);
            telemetry_slot_ = root_comp->findChildWithID("telemetry-slot");

            // Ensure ComboBoxes are populated (1-based item IDs == 0-based AudioParameterChoice index + 1)
            // and display the active choice label immediately upon editor creation.
            auto sync_choice_combo = [&](const char* param_id) {
                auto* combo = dynamic_cast<juce::ComboBox*>(root_comp->findChildWithID(param_id));
                auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor_.apvts().getParameter(param_id));
                if (combo == nullptr || choice == nullptr) return;
                if (combo->getNumItems() != choice->choices.size()) {
                    combo->clear(juce::dontSendNotification);
                    combo->addItemList(choice->choices, 1);
                }
                combo->setSelectedItemIndex(choice->getIndex(), juce::dontSendNotification);
                jassert(combo->getNumItems() == choice->choices.size());
                jassert(combo->getItemText(choice->getIndex()) == choice->getCurrentChoiceName());
            };
            sync_choice_combo("routing_mode");
            sync_choice_combo("output_dialect");
        }
    }

    if (telemetry_slot_ != nullptr) {
        telemetry_slot_->addAndMakeVisible(telemetry_meter_);
    } else {
        addAndMakeVisible(telemetry_meter_);
    }

    setSize(580, 370);
}

void GuitarTransducerAudioProcessorEditor::resized()
{
    if (root_item_ != nullptr) {
        if (auto* root_comp = root_item_->getComponent()) {
            root_comp->setBounds(getLocalBounds());
        }
    }
    if (telemetry_slot_ != nullptr) {
        telemetry_meter_.setBounds(telemetry_slot_->getLocalBounds());
    } else {
        telemetry_meter_.setBounds(getLocalBounds().reduced(16).removeFromTop(106));
    }
}

GuitarTransducerAudioProcessorEditor::~GuitarTransducerAudioProcessorEditor()
{
    if (root_item_ != nullptr) {
        if (auto* root_comp = root_item_->getComponent()) {
            root_comp->setLookAndFeel(nullptr);
        }
    }
    setLookAndFeel(nullptr);
}
