#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "AudioCore.h"
#include <array>
#include <optional>

namespace dualdeck
{
class WaveformDisplay final : public juce::Component
{
public:
    std::function<void(double, bool)> onMarkerMoved;
    std::function<void(double)> onScrub;
    std::function<void()> onScrubStart;
    std::function<void(double)> onScrubEnd;
    std::function<void()> onViewChanged;
    void setPeaks(std::vector<float> values, double seconds);
    void setTrim(double start, double end);
    void setPlaybackPosition(double seconds);
    void zoomBy(double factor, double focusSeconds = -1.0);
    void resetZoom();
    double getZoom() const { return zoom; }
    double getScrubPosition() const { return scrubPosition; }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void moveMarker(float x);
    void scrub(float x);
    double visibleDuration() const { return duration / zoom; }
    double secondsAt(float x) const;
    float xAt(double seconds) const;
    std::vector<float> peaks;
    double duration = 180.0, trimStart = 0.0, trimEnd = 0.0;
    double scrubPosition = -1.0;
    double zoom = 1.0, viewStart = 0.0;
    bool movingEnd = false, draggingMarker = false;
};

class PadEditorPanel final : public juce::Component
{
public:
    std::function<void(const VoiceSettings&, std::optional<bool>)> onSave;
    std::function<void()> onClose;
    std::function<bool(bool, double, double)> onPreviewLoop;
    std::function<void(double)> onPreviewScrub;
    std::function<void()> onScrubStart;
    std::function<void(double)> onScrubEnd;
    std::function<bool()> onPlay;
    std::function<void()> onStop;
    PadEditorPanel();
    void setPad(const juce::String& name, const VoiceSettings& settings, bool loopFromPad = false);
    void setWaveform(std::vector<float> peaks, double duration);
    void setPlaybackPosition(double seconds);
    void setMessage(const juce::String& message);
    VoiceSettings currentSettings() const;
    void resetPreview() { previewLoop.setToggleState(false, juce::dontSendNotification); }
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    struct ExtraEffect
    {
        juce::ToggleButton enabled;
        std::array<juce::Label, 3> labels;
        std::array<juce::Slider, 3> sliders;
        juce::Rectangle<int> card;
        int rows = 0;
    };
    VoiceSettings settings;
    WaveformDisplay waveform;
    juce::Label heading, help, message, audioHeading, positionLabel, zoomLabel, filterHeading, filterHint, highPassHeading, highPassHint, delayHeading, delayHint;
    juce::Label startLabel, endLabel, cutoffLabel, filterMixLabel, highPassCutoffLabel, highPassMixLabel, delayTimeLabel, delayFeedbackLabel, delayMixLabel;
    juce::Slider start, end, cutoff, filterMix, highPassCutoff, highPassMix, delayTime, delayFeedback, delayMix;
    juce::TextButton saveButton {"Salva impostazioni"}, closeButton {"Chiudi"};
    juce::TextButton playButton {"Riproduci file"}, stopPreviewButton {"Ferma"};
    juce::TextButton zoomOutButton {"-"}, zoomInButton {"+"}, zoomResetButton {"Tutto"};
    juce::ToggleButton previewLoop {"Ascolta loop A-B"}, padLoop {"Ripeti A-B premendo il pad"};
    juce::Rectangle<int> audioCard, filterCard, delayCard;
    std::array<ExtraEffect, 6> extraEffects;
    bool updating = false;
    bool originalPadLoop = false;
};
}
