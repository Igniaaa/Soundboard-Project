#include "PadEditor.h"
#include <algorithm>
#include <cmath>

namespace dualdeck
{
void WaveformDisplay::setPeaks(std::vector<float> values, double seconds)
{
    peaks = std::move(values);
    duration = std::max(0.01, seconds);
    scrubPosition = peaks.empty() ? -1.0 : 0.0;
    zoom = 1.0;
    viewStart = 0.0;
    if (onViewChanged) onViewChanged();
    repaint();
}
void WaveformDisplay::setTrim(double start, double end)
{
    trimStart = start; trimEnd = end; repaint();
}
void WaveformDisplay::setPlaybackPosition(double seconds)
{
    const auto position = std::clamp(seconds, 0.0, duration);
    if (scrubPosition != position)
    {
        scrubPosition = position;
        if (zoom > 1.0 && (position < viewStart || position > viewStart + visibleDuration()))
            viewStart = std::clamp(position - visibleDuration() * 0.25, 0.0, duration - visibleDuration());
        repaint();
    }
}
void WaveformDisplay::zoomBy(double factor, double focusSeconds)
{
    if (peaks.empty() || !std::isfinite(factor) || factor <= 0) return;
    const double focus = std::clamp(focusSeconds >= 0 ? focusSeconds
        : scrubPosition >= 0 ? scrubPosition : duration * 0.5, 0.0, duration);
    const double oldSpan = visibleDuration();
    const double fraction = std::clamp((focus - viewStart) / oldSpan, 0.0, 1.0);
    zoom = std::clamp(zoom * factor, 1.0, 32.0);
    viewStart = std::clamp(focus - fraction * visibleDuration(), 0.0, duration - visibleDuration());
    if (onViewChanged) onViewChanged();
    repaint();
}
void WaveformDisplay::resetZoom()
{
    zoom = 1.0;
    viewStart = 0.0;
    if (onViewChanged) onViewChanged();
    repaint();
}
double WaveformDisplay::secondsAt(float x) const
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    return std::clamp(viewStart + static_cast<double>((x - bounds.getX())
        / std::max(1.0f, bounds.getWidth())) * visibleDuration(), 0.0, duration);
}
float WaveformDisplay::xAt(double seconds) const
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    return bounds.getX() + bounds.getWidth()
        * static_cast<float>((seconds - viewStart) / visibleDuration());
}
void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colour(0xff101c2a)); g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff34465b));
    g.drawHorizontalLine(juce::roundToInt(bounds.getCentreY()), bounds.getX(), bounds.getRight());
    if (peaks.empty())
    {
        g.setColour(juce::Colour(0xffa9b9ca));
        g.drawFittedText("Caricamento della forma d'onda...", getLocalBounds(), juce::Justification::centred, 1);
        return;
    }
    const auto startX = xAt(trimStart);
    const auto endX = xAt(trimEnd > 0 ? trimEnd : duration);
    const auto middle = bounds.getCentreY();
    const int columns = std::max(1, juce::roundToInt(bounds.getWidth()));
    for (int column = 0; column < columns; ++column)
    {
        const double firstTime = viewStart + visibleDuration() * column / columns;
        const double lastTime = viewStart + visibleDuration() * (column + 1) / columns;
        const auto first = std::min(peaks.size() - 1, static_cast<std::size_t>(firstTime / duration * peaks.size()));
        const auto last = std::min(peaks.size(), std::max(first + 1,
            static_cast<std::size_t>(std::ceil(lastTime / duration * peaks.size()))));
        float peak = 0.0f;
        for (auto bin = first; bin < last; ++bin) peak = std::max(peak, peaks[bin]);
        const auto x = bounds.getX() + static_cast<float>(column);
        const auto amplitude = std::clamp(peak, 0.0f, 1.0f) * (bounds.getHeight() * 0.44f);
        g.setColour(x < startX || x > endX ? juce::Colour(0xff526474) : juce::Colour(0xff63dfc4));
        g.drawVerticalLine(juce::roundToInt(x), middle - amplitude, middle + amplitude);
    }
    g.setColour(juce::Colour(0xffffc579));
    if (startX >= bounds.getX() && startX <= bounds.getRight())
        g.drawLine(startX, bounds.getY(), startX, bounds.getBottom(), 2.0f);
    if (endX >= bounds.getX() && endX <= bounds.getRight())
        g.drawLine(endX, bounds.getY(), endX, bounds.getBottom(), 2.0f);
    if (scrubPosition >= 0)
    {
        const auto x = xAt(scrubPosition);
        if (x >= bounds.getX() && x <= bounds.getRight())
        {
            g.setColour(juce::Colour(0xffe7f1ff));
            g.drawLine(x, bounds.getY() + 6.0f, x, bounds.getBottom(), 2.0f);
            juce::Path head;
            head.addTriangle(x - 6.0f, bounds.getY(), x + 6.0f, bounds.getY(), x, bounds.getY() + 8.0f);
            g.fillPath(head);
        }
    }
    if (zoom > 1.0)
    {
        const auto overview = bounds.removeFromBottom(4.0f);
        g.setColour(juce::Colour(0xff34465b)); g.fillRect(overview);
        g.setColour(juce::Colour(0xff63dfc4));
        g.fillRect(overview.getX() + overview.getWidth() * static_cast<float>(viewStart / duration),
                   overview.getY(), overview.getWidth() / static_cast<float>(zoom), overview.getHeight());
    }
}
void WaveformDisplay::moveMarker(float x)
{
    if (!onMarkerMoved) return;
    onMarkerMoved(secondsAt(x), movingEnd);
}
void WaveformDisplay::scrub(float x)
{
    if (peaks.empty() || !onScrub) return;
    scrubPosition = secondsAt(x);
    repaint();
    onScrub(scrubPosition);
}
void WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const auto sx = xAt(trimStart);
    const auto ex = xAt(trimEnd > 0 ? trimEnd : duration);
    const auto startDistance = std::abs(event.position.x - sx);
    const auto endDistance = std::abs(event.position.x - ex);
    draggingMarker = event.position.y > bounds.getY() + 22.0f
        && std::min(startDistance, endDistance) <= 12.0f;
    movingEnd = endDistance < startDistance;
    if (draggingMarker) moveMarker(event.position.x);
    else
    {
        if (!peaks.empty() && onScrubStart) onScrubStart();
        scrub(event.position.x);
    }
}
void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingMarker) moveMarker(event.position.x);
    else scrub(event.position.x);
}
void WaveformDisplay::mouseUp(const juce::MouseEvent& event)
{
    if (!draggingMarker && !peaks.empty())
    {
        const auto seconds = secondsAt(event.position.x);
        if (seconds != scrubPosition) scrub(event.position.x);
        if (onScrubEnd) onScrubEnd(scrubPosition);
    }
    draggingMarker = false;
}
void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (peaks.empty()) return;
    if (event.mods.isShiftDown())
    {
        viewStart = std::clamp(viewStart - wheel.deltaY * visibleDuration() * 0.35,
                               0.0, duration - visibleDuration());
        repaint();
    }
    else zoomBy(std::pow(2.0, wheel.deltaY * 4.0), secondsAt(event.position.x));
}

PadEditorPanel::PadEditorPanel()
{
    setWantsKeyboardFocus(true);
    for (auto* component : std::initializer_list<juce::Component*>{&waveform, &heading, &help, &message,
                            &positionLabel, &zoomLabel, &zoomOutButton, &zoomInButton, &zoomResetButton,
                            &audioHeading, &filterHeading, &filterHint, &highPassHeading, &highPassHint, &delayHeading, &delayHint,
                            &startLabel, &endLabel, &cutoffLabel, &filterMixLabel, &highPassCutoffLabel, &highPassMixLabel,
                            &delayTimeLabel, &delayFeedbackLabel, &delayMixLabel,
                            &start, &end, &cutoff, &filterMix, &highPassCutoff, &highPassMix, &delayTime, &delayFeedback, &delayMix,
                            &previewLoop, &padLoop, &playButton, &stopPreviewButton, &saveButton, &closeButton}) addAndMakeVisible(component);
    heading.setFont(juce::FontOptions(23.0f));
    help.setText("Tieni premuta o trascina l'onda per ascoltare. L'anteprima usa gli effetti correnti.", juce::dontSendNotification);
    positionLabel.setText("0.00 s", juce::dontSendNotification);
    positionLabel.setJustificationType(juce::Justification::centredRight);
    zoomLabel.setText("1×", juce::dontSendNotification);
    zoomLabel.setJustificationType(juce::Justification::centred);
    zoomOutButton.setTooltip("Riduci lo zoom della forma d'onda.");
    zoomInButton.setTooltip("Ingrandisci la forma d'onda intorno alla testina.");
    zoomResetButton.setTooltip("Mostra l'intero file.");
    zoomOutButton.setTitle("Riduci zoom");
    zoomInButton.setTitle("Aumenta zoom");
    zoomResetButton.setTitle("Mostra l'intero file");
    zoomOutButton.onClick = [this] { waveform.zoomBy(0.5); };
    zoomInButton.onClick = [this] { waveform.zoomBy(2.0); };
    zoomResetButton.onClick = [this] { waveform.resetZoom(); };
    waveform.onViewChanged = [this]
    {
        zoomLabel.setText(juce::String(waveform.getZoom(), 1) + "×", juce::dontSendNotification);
    };
    audioHeading.setText("AUDIO E LOOP A-B", juce::dontSendNotification);
    filterHeading.setText("FILTRO PASSA BASSO", juce::dontSendNotification);
    filterHint.setText("Taglia gli acuti. Mix 0% = spento.", juce::dontSendNotification);
    highPassHeading.setText("FILTRO PASSA ALTO", juce::dontSendNotification);
    highPassHint.setText("Taglia i bassi. Mix 0% = spento.", juce::dontSendNotification);
    delayHeading.setText("DELAY", juce::dontSendNotification);
    delayHint.setText("Ripetizioni ritardate; il segnale originale resta udibile.", juce::dontSendNotification);
    startLabel.setText("A - inizio (s)", juce::dontSendNotification);
    endLabel.setText("B - fine (s), 0 = fine file", juce::dontSendNotification);
    previewLoop.setTooltip("Ascolta ripetutamente il tratto tra A e B senza salvare.");
    previewLoop.setEnabled(false);
    playButton.setEnabled(false);
    stopPreviewButton.setEnabled(false);
    padLoop.setTooltip("Salva il pad in modalità Loop on/off sul tratto A-B.");
    cutoffLabel.setText("Frequenza", juce::dontSendNotification);
    filterMixLabel.setText("Mix", juce::dontSendNotification);
    highPassCutoffLabel.setText("Frequenza", juce::dontSendNotification);
    highPassMixLabel.setText("Mix", juce::dontSendNotification);
    delayTimeLabel.setText("Tempo", juce::dontSendNotification);
    delayFeedbackLabel.setText("Feedback", juce::dontSendNotification);
    delayMixLabel.setText("Mix", juce::dontSendNotification);
    for (auto* label : {&audioHeading, &filterHeading, &highPassHeading, &delayHeading})
    {
        label->setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff63dfc4));
    }
    for (auto* label : {&help, &message, &positionLabel, &zoomLabel, &filterHint, &highPassHint, &delayHint})
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa9b9ca));
    for (auto* slider : {&start, &end, &cutoff, &filterMix, &highPassCutoff, &highPassMix, &delayTime, &delayFeedback, &delayMix})
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
    }
    const std::array<juce::String, 6> names {"Riverbero", "Compressore", "Tremolo", "Distorsione", "Ping-pong", "Chorus"};
    const std::array<std::array<juce::String, 3>, 6> rowNames {{
        {{"Durata", "Mix", ""}}, {{"Soglia", "Rapporto", ""}}, {{"Velocita", "Profondita", ""}},
        {{"Drive", "", ""}}, {{"Tempo", "Feedback", "Mix"}}, {{"Velocita", "Profondita", "Mix"}}
    }};
    const std::array<int, 6> counts {2, 2, 2, 1, 3, 3};
    for (std::size_t effect = 0; effect < extraEffects.size(); ++effect)
    {
        auto& item = extraEffects[effect];
        item.rows = counts[effect];
        item.enabled.setButtonText(names[effect]);
        item.enabled.setTitle(names[effect] + ": attiva effetto");
        addAndMakeVisible(item.enabled);
        for (int row = 0; row < item.rows; ++row)
        {
            item.labels[static_cast<std::size_t>(row)].setText(rowNames[effect][static_cast<std::size_t>(row)], juce::dontSendNotification);
            auto& slider = item.sliders[static_cast<std::size_t>(row)];
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
            addAndMakeVisible(item.labels[static_cast<std::size_t>(row)]);
            addAndMakeVisible(slider);
        }
    }
    auto range = [this](int effect, int row, double minimum, double maximum, double interval, const juce::String& suffix)
    {
        auto& slider = extraEffects[static_cast<std::size_t>(effect)].sliders[static_cast<std::size_t>(row)];
        slider.setRange(minimum, maximum, interval);
        slider.setTextValueSuffix(suffix);
    };
    range(0, 0, 0.2, 2.0, 0.01, " s"); range(0, 1, 0, 100, 1, " %");
    range(1, 0, -48, 0, 1, " dB"); range(1, 1, 1, 20, 0.1, ":1");
    range(2, 0, 0.1, 16, 0.1, " Hz"); range(2, 1, 0, 100, 1, " %");
    range(3, 0, 0, 100, 1, "");
    range(4, 0, 50, 1000, 1, " ms"); range(4, 1, 0, 80, 1, " %"); range(4, 2, 0, 100, 1, " %");
    range(5, 0, 0.1, 10, 0.1, " Hz"); range(5, 1, 0, 100, 1, " %"); range(5, 2, 0, 100, 1, " %");
    for (const int effect : {0, 4, 5})
        extraEffects[static_cast<std::size_t>(effect)].enabled.onClick = [this, effect]
        {
            auto& item = extraEffects[static_cast<std::size_t>(effect)];
            const int mixRow = item.rows - 1;
            if (item.enabled.getToggleState() && item.sliders[static_cast<std::size_t>(mixRow)].getValue() == 0)
                item.sliders[static_cast<std::size_t>(mixRow)].setValue(30);
        };
    start.setRange(0, 180, 0.01); end.setRange(0, 180, 0.01);
    cutoff.setRange(100, 18000, 1); cutoff.setSkewFactorFromMidPoint(2000);
    filterMix.setRange(0, 100, 1);
    highPassCutoff.setRange(100, 18000, 1); highPassCutoff.setSkewFactorFromMidPoint(2000);
    highPassMix.setRange(0, 100, 1);
    delayTime.setRange(50, 1000, 10);
    delayFeedback.setRange(0, 80, 1);
    delayMix.setRange(0, 100, 1);
    cutoff.setTextValueSuffix(" Hz");
    highPassCutoff.setTextValueSuffix(" Hz");
    delayTime.setTextValueSuffix(" ms");
    for (auto* slider : {&filterMix, &highPassMix, &delayFeedback, &delayMix}) slider->setTextValueSuffix(" %");
    cutoff.setTitle("Passa basso: frequenza di taglio");
    filterMix.setTitle("Passa basso: mix del segnale filtrato");
    highPassCutoff.setTitle("Passa alto: frequenza di taglio");
    highPassMix.setTitle("Passa alto: mix del segnale filtrato");
    delayTime.setTitle("Delay: tempo delle ripetizioni");
    delayFeedback.setTitle("Delay: feedback delle ripetizioni");
    delayMix.setTitle("Delay: mix delle ripetizioni");
    const auto changedBounds = [this]
    {
        if (updating) return;
        waveform.setTrim(start.getValue(), end.getValue());
        if (previewLoop.getToggleState())
        {
            previewLoop.setToggleState(false, juce::dontSendNotification);
            if (onPreviewLoop) onPreviewLoop(false, 0, 0);
        }
    };
    start.onValueChange = changedBounds;
    end.onValueChange = changedBounds;
    cutoff.onValueChange = [this] { if (!updating && filterMix.getValue() == 0) filterMix.setValue(100); };
    highPassCutoff.onValueChange = [this] { if (!updating && highPassMix.getValue() == 0) highPassMix.setValue(100); };
    waveform.onMarkerMoved = [this](double value, bool moveEnd)
    {
        if (moveEnd) end.setValue(value >= end.getMaximum() - 0.01 ? 0.0 : std::max(value, start.getValue() + 0.01));
        else start.setValue(std::min(value, end.getValue() > 0 ? end.getValue() - 0.01 : end.getMaximum()));
    };
    waveform.onScrub = [this](double seconds)
    {
        positionLabel.setText(juce::String(seconds, 2) + " s", juce::dontSendNotification);
        if (previewLoop.getToggleState())
        {
            previewLoop.setToggleState(false, juce::dontSendNotification);
            if (onPreviewLoop) onPreviewLoop(false, 0, 0);
        }
        if (onPreviewScrub) onPreviewScrub(seconds);
    };
    waveform.onScrubStart = [this] { if (onScrubStart) onScrubStart(); };
    waveform.onScrubEnd = [this](double seconds) { if (onScrubEnd) onScrubEnd(seconds); };
    playButton.onClick = [this]
    {
        if (previewLoop.getToggleState())
        {
            previewLoop.setToggleState(false, juce::dontSendNotification);
            if (onPreviewLoop) onPreviewLoop(false, 0, 0);
        }
        if (onPlay && !onPlay())
            message.setText("Riproduzione non disponibile: controlla file e uscita audio.", juce::dontSendNotification);
    };
    stopPreviewButton.onClick = [this]
    {
        previewLoop.setToggleState(false, juce::dontSendNotification);
        if (onStop) onStop();
    };
    previewLoop.onClick = [this]
    {
        const auto enabled = previewLoop.getToggleState();
        const auto loopEnd = end.getValue() > 0 ? end.getValue() : end.getMaximum();
        if (enabled && loopEnd - start.getValue() < 0.01)
        {
            previewLoop.setToggleState(false, juce::dontSendNotification);
            message.setText("B deve essere dopo A.", juce::dontSendNotification);
            return;
        }
        if (onPreviewLoop && !onPreviewLoop(enabled, start.getValue(), end.getValue()))
        {
            previewLoop.setToggleState(false, juce::dontSendNotification);
            message.setText("Anteprima non disponibile: controlla file e uscita audio.", juce::dontSendNotification);
        }
    };
    saveButton.onClick = [this]
    {
        const auto loopEnd = end.getValue() > 0 ? end.getValue() : end.getMaximum();
        if (loopEnd - start.getValue() < 0.01)
        {
            message.setText("B deve essere dopo A.", juce::dontSendNotification);
            return;
        }
        auto changed = currentSettings();
        std::optional<bool> loopChange;
        if (padLoop.getToggleState() != originalPadLoop) loopChange = padLoop.getToggleState();
        if (onSave) onSave(changed, loopChange);
    };
    closeButton.onClick = [this]
    {
        if (previewLoop.getToggleState() && onPreviewLoop) onPreviewLoop(false, 0, 0);
        previewLoop.setToggleState(false, juce::dontSendNotification);
        if (onClose) onClose();
    };
}
VoiceSettings PadEditorPanel::currentSettings() const
{
    auto changed = settings;
    changed.trimStart = start.getValue(); changed.trimEnd = end.getValue();
    changed.filterCutoffHz = static_cast<float>(cutoff.getValue());
    changed.filterMix = static_cast<float>(filterMix.getValue() / 100.0);
    changed.highPassCutoffHz = static_cast<float>(highPassCutoff.getValue());
    changed.highPassMix = static_cast<float>(highPassMix.getValue() / 100.0);
    changed.delayTimeMs = static_cast<float>(delayTime.getValue());
    changed.delayFeedback = static_cast<float>(delayFeedback.getValue() / 100.0);
    changed.delayMix = static_cast<float>(delayMix.getValue() / 100.0);
    auto slider = [this](int effect, int row) { return extraEffects[static_cast<std::size_t>(effect)].sliders[static_cast<std::size_t>(row)].getValue(); };
    changed.reverbDecay = static_cast<float>(slider(0, 0));
    changed.reverbMix = extraEffects[0].enabled.getToggleState() ? static_cast<float>(slider(0, 1) / 100.0) : 0.0f;
    changed.compressorEnabled = extraEffects[1].enabled.getToggleState();
    changed.compressorThresholdDb = static_cast<float>(slider(1, 0));
    changed.compressorRatio = static_cast<float>(slider(1, 1));
    changed.tremoloEnabled = extraEffects[2].enabled.getToggleState();
    changed.tremoloSpeedHz = static_cast<float>(slider(2, 0));
    changed.tremoloDepth = static_cast<float>(slider(2, 1) / 100.0);
    changed.distortionEnabled = extraEffects[3].enabled.getToggleState();
    changed.distortionDrive = static_cast<float>(slider(3, 0));
    changed.pingPongTimeMs = static_cast<float>(slider(4, 0));
    changed.pingPongFeedback = static_cast<float>(slider(4, 1) / 100.0);
    changed.pingPongMix = extraEffects[4].enabled.getToggleState() ? static_cast<float>(slider(4, 2) / 100.0) : 0.0f;
    changed.chorusSpeedHz = static_cast<float>(slider(5, 0));
    changed.chorusDepth = static_cast<float>(slider(5, 1) / 100.0);
    changed.chorusMix = extraEffects[5].enabled.getToggleState() ? static_cast<float>(slider(5, 2) / 100.0) : 0.0f;
    return changed;
}
void PadEditorPanel::setPad(const juce::String& name, const VoiceSettings& value, bool loopFromPad)
{
    settings = value; updating = true;
    waveform.setPeaks({}, 180.0);
    positionLabel.setText("0.00 s", juce::dontSendNotification);
    if (previewLoop.getToggleState() && onPreviewLoop) onPreviewLoop(false, 0, 0);
    previewLoop.setToggleState(false, juce::dontSendNotification);
    previewLoop.setEnabled(false);
    playButton.setEnabled(false);
    stopPreviewButton.setEnabled(false);
    originalPadLoop = loopFromPad;
    padLoop.setToggleState(loopFromPad, juce::dontSendNotification);
    heading.setText("Audio ed effetti: " + name, juce::dontSendNotification);
    waveform.setPeaks({}, 180.0);
    start.setRange(0, 180, 0.01); end.setRange(0, 180, 0.01);
    start.setValue(value.trimStart, juce::dontSendNotification);
    end.setValue(value.trimEnd, juce::dontSendNotification);
    cutoff.setValue(value.filterCutoffHz, juce::dontSendNotification);
    filterMix.setValue(value.filterMix * 100.0f, juce::dontSendNotification);
    highPassCutoff.setValue(value.highPassCutoffHz, juce::dontSendNotification);
    highPassMix.setValue(value.highPassMix * 100.0f, juce::dontSendNotification);
    delayTime.setValue(value.delayTimeMs, juce::dontSendNotification);
    delayFeedback.setValue(value.delayFeedback * 100.0f, juce::dontSendNotification);
    delayMix.setValue(value.delayMix * 100.0f, juce::dontSendNotification);
    const auto set = [this](int effect, int row, double amount)
    { extraEffects[static_cast<std::size_t>(effect)].sliders[static_cast<std::size_t>(row)].setValue(amount, juce::dontSendNotification); };
    extraEffects[0].enabled.setToggleState(value.reverbMix > 0, juce::dontSendNotification);
    set(0, 0, value.reverbDecay); set(0, 1, value.reverbMix * 100.0f);
    extraEffects[1].enabled.setToggleState(value.compressorEnabled, juce::dontSendNotification);
    set(1, 0, value.compressorThresholdDb); set(1, 1, value.compressorRatio);
    extraEffects[2].enabled.setToggleState(value.tremoloEnabled, juce::dontSendNotification);
    set(2, 0, value.tremoloSpeedHz); set(2, 1, value.tremoloDepth * 100.0f);
    extraEffects[3].enabled.setToggleState(value.distortionEnabled, juce::dontSendNotification);
    set(3, 0, value.distortionDrive);
    extraEffects[4].enabled.setToggleState(value.pingPongMix > 0, juce::dontSendNotification);
    set(4, 0, value.pingPongTimeMs); set(4, 1, value.pingPongFeedback * 100.0f); set(4, 2, value.pingPongMix * 100.0f);
    extraEffects[5].enabled.setToggleState(value.chorusMix > 0, juce::dontSendNotification);
    set(5, 0, value.chorusSpeedHz); set(5, 1, value.chorusDepth * 100.0f); set(5, 2, value.chorusMix * 100.0f);
    waveform.setTrim(value.trimStart, value.trimEnd);
    message.setText("Caricamento...", juce::dontSendNotification);
    updating = false;
}
void PadEditorPanel::setWaveform(std::vector<float> peaks, double duration)
{
    updating = true;
    previewLoop.setEnabled(!peaks.empty());
    playButton.setEnabled(!peaks.empty());
    stopPreviewButton.setEnabled(!peaks.empty());
    start.setRange(0, duration, 0.01); end.setRange(0, duration, 0.01);
    waveform.setPeaks(std::move(peaks), duration);
    waveform.setTrim(start.getValue(), end.getValue());
    message.setText(juce::String(duration, 2) + " s  /  Rotella: zoom; Maiusc+rotella: sposta; giallo: A-B.", juce::dontSendNotification);
    updating = false;
}
void PadEditorPanel::setPlaybackPosition(double seconds)
{
    waveform.setPlaybackPosition(seconds);
    positionLabel.setText(juce::String(seconds, 2) + " s", juce::dontSendNotification);
}
void PadEditorPanel::setMessage(const juce::String& value) { message.setText(value, juce::dontSendNotification); }
void PadEditorPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff121c29)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
    for (const auto& card : {audioCard, filterCard, delayCard})
    {
        g.setColour(juce::Colour(0xff182536)); g.fillRoundedRectangle(card.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff34465b)); g.drawRoundedRectangle(card.toFloat().reduced(0.5f), 10.0f, 1.0f);
    }
    for (const auto& effect : extraEffects)
    {
        g.setColour(juce::Colour(0xff182536)); g.fillRoundedRectangle(effect.card.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff34465b)); g.drawRoundedRectangle(effect.card.toFloat().reduced(0.5f), 10.0f, 1.0f);
    }
}
void PadEditorPanel::resized()
{
    auto area = getLocalBounds().reduced(24);
    heading.setBounds(area.removeFromTop(36));
    help.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);
    audioCard = area.removeFromTop(350);
    int right = audioCard.getRight() - 16;
    zoomResetButton.setBounds(right - 58, audioCard.getY() + 11, 58, 24); right -= 64;
    zoomInButton.setBounds(right - 28, audioCard.getY() + 11, 28, 24); right -= 34;
    zoomOutButton.setBounds(right - 28, audioCard.getY() + 11, 28, 24); right -= 34;
    zoomLabel.setBounds(right - 54, audioCard.getY() + 11, 54, 24); right -= 60;
    positionLabel.setBounds(right - 84, audioCard.getY() + 11, 84, 24); right -= 92;
    audioHeading.setBounds(audioCard.getX() + 16, audioCard.getY() + 11,
                           std::max(80, right - audioCard.getX() - 16), 24);
    waveform.setBounds(audioCard.getX() + 16, audioCard.getY() + 40, audioCard.getWidth() - 32, 132);
    message.setBounds(audioCard.getX() + 16, audioCard.getY() + 177, audioCard.getWidth() - 32, 24);
    auto audioRow = [&](juce::Label& label, juce::Slider& slider, int y)
    {
        label.setBounds(audioCard.getX() + 16, audioCard.getY() + y, 160, 28);
        slider.setBounds(audioCard.getX() + 176, audioCard.getY() + y, audioCard.getWidth() - 192, 28);
    };
    audioRow(startLabel, start, 202); audioRow(endLabel, end, 231);
    previewLoop.setBounds(audioCard.getX() + 16, audioCard.getY() + 265, 230, 28);
    padLoop.setBounds(audioCard.getX() + 260, audioCard.getY() + 265, 300, 28);
    playButton.setBounds(audioCard.getX() + 16, audioCard.getY() + 302, 150, 32);
    stopPreviewButton.setBounds(audioCard.getX() + 176, audioCard.getY() + 302, 100, 32);
    area.removeFromTop(14);
    auto effects = area.removeFromTop(246);
    filterCard = effects.removeFromLeft((effects.getWidth() - 14) / 2);
    effects.removeFromLeft(14);
    delayCard = effects;
    auto layoutEffect = [](const juce::Rectangle<int>& card, juce::Label& title, juce::Label& hint,
                           std::initializer_list<std::pair<juce::Label*, juce::Slider*>> controls)
    {
        title.setBounds(card.getX() + 16, card.getY() + 12, card.getWidth() - 32, 24);
        hint.setBounds(card.getX() + 16, card.getY() + 39, card.getWidth() - 32, 36);
        int y = card.getY() + 83;
        for (const auto& control : controls)
        {
            control.first->setBounds(card.getX() + 16, y, 86, 32);
            control.second->setBounds(card.getX() + 102, y, card.getWidth() - 118, 32);
            y += 43;
        }
    };
    filterHeading.setBounds(filterCard.getX() + 16, filterCard.getY() + 10, filterCard.getWidth() - 32, 22);
    filterHint.setBounds(filterCard.getX() + 16, filterCard.getY() + 34, filterCard.getWidth() - 32, 24);
    auto filterRow = [this](juce::Label& label, juce::Slider& slider, int y)
    {
        label.setBounds(filterCard.getX() + 16, filterCard.getY() + y, 86, 30);
        slider.setBounds(filterCard.getX() + 102, filterCard.getY() + y, filterCard.getWidth() - 118, 30);
    };
    filterRow(cutoffLabel, cutoff, 62);
    filterRow(filterMixLabel, filterMix, 94);
    highPassHeading.setBounds(filterCard.getX() + 16, filterCard.getY() + 130, filterCard.getWidth() - 32, 22);
    highPassHint.setBounds(filterCard.getX() + 16, filterCard.getY() + 154, filterCard.getWidth() - 32, 20);
    filterRow(highPassCutoffLabel, highPassCutoff, 176);
    filterRow(highPassMixLabel, highPassMix, 208);
    layoutEffect(delayCard, delayHeading, delayHint,
                 {{&delayTimeLabel, &delayTime}, {&delayFeedbackLabel, &delayFeedback}, {&delayMixLabel, &delayMix}});
    area.removeFromTop(14);
    for (std::size_t index = 0; index < extraEffects.size(); index += 2)
    {
        auto row = area.removeFromTop(190);
        extraEffects[index].card = row.removeFromLeft((row.getWidth() - 14) / 2);
        row.removeFromLeft(14);
        extraEffects[index + 1].card = row;
        for (std::size_t column = 0; column < 2; ++column)
        {
            auto& item = extraEffects[index + column];
            item.enabled.setBounds(item.card.getX() + 16, item.card.getY() + 10, item.card.getWidth() - 32, 28);
            for (int control = 0; control < item.rows; ++control)
            {
                const int y = item.card.getY() + 44 + control * 44;
                item.labels[static_cast<std::size_t>(control)].setBounds(item.card.getX() + 16, y, 88, 30);
                item.sliders[static_cast<std::size_t>(control)].setBounds(item.card.getX() + 104, y, item.card.getWidth() - 120, 30);
            }
        }
        area.removeFromTop(14);
    }
    area.removeFromTop(4);
    auto buttons = area.removeFromTop(42);
    closeButton.setBounds(buttons.removeFromRight(130));
    buttons.removeFromRight(10);
    saveButton.setBounds(buttons.removeFromRight(180));
}
bool PadEditorPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}
}
