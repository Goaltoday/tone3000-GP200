#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <vector>
namespace gp200 { struct GP200IRUpload { std::vector<juce::MidiMessage> chunks; juce::MidiMessage prepareMessage, commitMessage; juce::String displayName; }; }
