#include "RoomView3D.h"

namespace spatial
{

RoomView3D::RoomView3D (AudioEngine& e) : engine (e)
{
    for (auto* b : { &perspectiveButton, &topButton, &frontButton, &sideButton })
    {
        addAndMakeVisible (b);
        b->setColour (juce::TextButton::buttonColourId, colours::panel.withAlpha (0.85f));
        b->setColour (juce::TextButton::textColourOffId, colours::text);
    }

    perspectiveButton.onClick = [this] { setCameraPreset (CameraPreset::perspective); };
    topButton.onClick         = [this] { setCameraPreset (CameraPreset::top); };
    frontButton.onClick       = [this] { setCameraPreset (CameraPreset::front); };
    sideButton.onClick        = [this] { setCameraPreset (CameraPreset::side); };

    setOpaque (true);
    startTimerHz (30);
}

void RoomView3D::setRoom (const RoomLayout& newRoom)
{
    const bool sizeChanged = std::abs (newRoom.width - room.width) > 0.001f || std::abs (newRoom.depth - room.depth) > 0.001f;
    room = newRoom;

    if (sizeChanged && dragMode == DragMode::none)
        distance = juce::jmax (room.width, room.depth) * 2.0f + 4.0f;

    if (selectedSpeaker >= (int) room.speakers.size())
        selectedSpeaker = -1;

    repaint();
}

void RoomView3D::setPath (const PathParams& p)
{
    path = p;
    repaint();
}

void RoomView3D::setSelectedSpeaker (int index)
{
    selectedSpeaker = index;
    repaint();
}

void RoomView3D::setCameraPreset (CameraPreset preset)
{
    switch (preset)
    {
        case CameraPreset::perspective: yawDegrees = -25.0f; pitchDegrees = 35.0f; break;
        case CameraPreset::top:         yawDegrees = 0.0f;   pitchDegrees = 89.5f; break;
        case CameraPreset::front:       yawDegrees = 0.0f;   pitchDegrees = 4.0f;  break;
        case CameraPreset::side:        yawDegrees = 90.0f;  pitchDegrees = 4.0f;  break;
    }

    distance = juce::jmax (room.width, room.depth) * 2.0f + 4.0f;
    repaint();
}

void RoomView3D::timerCallback()
{
    repaint();
}

void RoomView3D::resized()
{
    auto area = getLocalBounds().reduced (10).removeFromTop (28);
    for (auto* b : { &sideButton, &frontButton, &topButton, &perspectiveButton })
    {
        b->setBounds (area.removeFromRight (58));
        area.removeFromRight (6);
    }
}

//==============================================================================
RoomView3D::Camera RoomView3D::makeCamera() const
{
    Camera cam;
    const Vec3 target { 0.0f, 0.0f, room.height * 0.35f };
    const float yaw = degreesToRadians (yawDegrees);
    const float pitch = degreesToRadians (pitchDegrees);

    cam.eye = target + Vec3 { std::cos (pitch) * std::sin (yaw),
                              -std::cos (pitch) * std::cos (yaw),
                              std::sin (pitch) } * distance;

    cam.forward = (target - cam.eye).normalised();
    Vec3 worldUp { 0.0f, 0.0f, 1.0f };

    // Looking straight down: use "front of the room" as up on screen
    if (std::abs (cam.forward.z) > 0.999f)
        worldUp = { std::sin (yaw), std::cos (yaw), 0.0f };

    cam.right = cam.forward.cross (worldUp).normalised();
    cam.up = cam.right.cross (cam.forward);
    cam.focal = (float) juce::jmin (getWidth(), getHeight()) * 1.25f;
    cam.centre = getLocalBounds().toFloat().getCentre();
    return cam;
}

RoomView3D::Projected RoomView3D::project (const Camera& cam, const Vec3& p) const
{
    const Vec3 d = p - cam.eye;
    const float z = d.dot (cam.forward);
    Projected result;
    result.depth = z;

    if (z < 0.1f)
        return result;

    result.point = { cam.centre.x + d.dot (cam.right) / z * cam.focal,
                     cam.centre.y - d.dot (cam.up) / z * cam.focal };
    result.visible = true;
    return result;
}

bool RoomView3D::rayHitsPlane (const Camera& cam, juce::Point<float> screen, const Vec3& planePoint,
                               const Vec3& planeNormal, Vec3& hit) const
{
    const float a = (screen.x - cam.centre.x) / cam.focal;
    const float b = -(screen.y - cam.centre.y) / cam.focal;
    const Vec3 dir = (cam.forward + cam.right * a + cam.up * b).normalised();

    const float denom = dir.dot (planeNormal);
    if (std::abs (denom) < 1.0e-4f)
        return false;

    const float t = (planePoint - cam.eye).dot (planeNormal) / denom;
    if (t <= 0.0f)
        return false;

    hit = cam.eye + dir * t;
    return true;
}

void RoomView3D::drawLine3D (juce::Graphics& g, const Camera& cam, const Vec3& a, const Vec3& b) const
{
    const auto pa = project (cam, a);
    const auto pb = project (cam, b);
    if (pa.visible && pb.visible)
        g.drawLine ({ pa.point, pb.point });
}

std::vector<int> RoomView3D::speakersAt (juce::Point<float> screen) const
{
    const auto cam = makeCamera();
    std::vector<std::pair<float, int>> hits;

    for (int i = 0; i < (int) room.speakers.size(); ++i)
    {
        const auto p = project (cam, room.speakers[(size_t) i].position);
        if (! p.visible)
            continue;

        const float radius = juce::jmax (9.0f, cam.focal * 0.2f / p.depth);
        if (p.point.getDistanceFrom (screen) <= radius)
            hits.push_back ({ p.depth, i });
    }

    std::sort (hits.begin(), hits.end());
    std::vector<int> result;
    for (const auto& h : hits)
        result.push_back (h.second);
    return result;
}

//==============================================================================
void RoomView3D::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colours::background.brighter (0.06f), bounds.getCentreX(), bounds.getCentreY(),
                                             colours::background, bounds.getRight(), bounds.getBottom(), true));
    g.fillAll();

    const auto cam = makeCamera();
    const float hw = room.width * 0.5f, hd = room.depth * 0.5f, h = room.height;

    // Floor grid, one line per metre
    g.setColour (colours::grid);
    for (float x = std::ceil (-hw); x <= hw; x += 1.0f)
        drawLine3D (g, cam, { x, -hd, 0.0f }, { x, hd, 0.0f });
    for (float y = std::ceil (-hd); y <= hd; y += 1.0f)
        drawLine3D (g, cam, { -hw, y, 0.0f }, { hw, y, 0.0f });

    // Room box
    g.setColour (colours::roomEdge);
    const Vec3 c[8] = { { -hw, -hd, 0 }, { hw, -hd, 0 }, { hw, hd, 0 }, { -hw, hd, 0 },
                        { -hw, -hd, h }, { hw, -hd, h }, { hw, hd, h }, { -hw, hd, h } };
    for (int i = 0; i < 4; ++i)
    {
        drawLine3D (g, cam, c[i], c[(i + 1) % 4]);
        drawLine3D (g, cam, c[i + 4], c[(i + 1) % 4 + 4]);
        drawLine3D (g, cam, c[i], c[i + 4]);
    }

    // Front marker
    {
        g.setColour (colours::roomEdge.brighter (0.5f));
        drawLine3D (g, cam, { -0.5f, hd, 0.0f }, { 0.0f, hd - 0.6f, 0.0f });
        drawLine3D (g, cam, { 0.5f, hd, 0.0f }, { 0.0f, hd - 0.6f, 0.0f });
        const auto label = project (cam, { 0.0f, hd, 0.0f });
        if (label.visible)
        {
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("FRONT", juce::Rectangle<float> (80.0f, 16.0f).withCentre (label.point.translated (0.0f, 12.0f)),
                        juce::Justification::centred);
        }
    }

    // Pathway
    if (path.shape != PathShape::none)
    {
        juce::Path line;
        bool started = false;
        const int steps = 360;
        for (int i = 0; i <= steps; ++i)
        {
            const auto p = project (cam, pathPoint (path, (float) i / (float) steps));
            if (! p.visible) { started = false; continue; }
            if (! started) { line.startNewSubPath (p.point); started = true; }
            else line.lineTo (p.point);
        }
        g.setColour (colours::pathLine.withAlpha (0.45f));
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Speakers and the source, drawn far-to-near
    struct Item { float depth; int speakerIndex; }; // speakerIndex -1 = source
    std::vector<Item> items;
    const Vec3 source = engine.getSourcePosition();

    for (int i = 0; i < (int) room.speakers.size(); ++i)
    {
        const auto p = project (cam, room.speakers[(size_t) i].position);
        if (p.visible)
            items.push_back ({ p.depth, i });
    }

    const auto sourceProjected = project (cam, source);
    if (sourceProjected.visible)
        items.push_back ({ sourceProjected.depth, -1 });

    std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.depth > b.depth; });

    const int testIndex = engine.isSpeakerTesting() ? engine.getSpeakerTestIndex() : -1;
    const int deviceOutputs = engine.getNumDeviceOutputs();

    for (const auto& item : items)
    {
        if (item.speakerIndex < 0)
        {
            // Drop line and floor shadow help show height
            g.setColour (colours::source.withAlpha (0.35f));
            drawLine3D (g, cam, source, { source.x, source.y, 0.0f });
            const auto floor = project (cam, { source.x, source.y, 0.0f });
            if (floor.visible)
            {
                const float r = cam.focal * 0.15f / floor.depth;
                g.setColour (colours::source.withAlpha (0.18f));
                g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 0.9f).withCentre (floor.point));
            }

            const float r = cam.focal * 0.2f / sourceProjected.depth;
            const auto centre = sourceProjected.point;
            g.setGradientFill (juce::ColourGradient (colours::source.withAlpha (0.45f), centre,
                                                     colours::source.withAlpha (0.0f), centre.translated (r * 3.0f, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (centre));
            g.setGradientFill (juce::ColourGradient (colours::source.brighter (0.7f), centre.translated (-r * 0.35f, -r * 0.35f),
                                                     colours::source.darker (0.4f), centre.translated (r, r), true));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
            continue;
        }

        const auto& spk = room.speakers[(size_t) item.speakerIndex];
        const auto p = project (cam, spk.position);
        const auto colour = colours::forSpeaker (spk.type);
        const bool selected = item.speakerIndex == selectedSpeaker;
        const bool testing = item.speakerIndex == testIndex;
        const bool unreachable = deviceOutputs > 0 && spk.output > deviceOutputs;
        const float level = juce::jlimit (0.0f, 1.0f, testing ? 1.0f : engine.getSpeakerLevel (item.speakerIndex));

        // Faint line to the floor
        g.setColour (colour.withAlpha (0.18f));
        drawLine3D (g, cam, spk.position, { spk.position.x, spk.position.y, 0.0f });

        const float base = cam.focal * (spk.type == SpeakerType::sub ? 0.24f : 0.17f) / p.depth;
        const float r = juce::jlimit (5.0f, 40.0f, base);

        // Glow shows how much of the sound this speaker is getting
        if (level > 0.01f)
        {
            const float glowR = r * (1.6f + level * 2.2f);
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.75f * level), p.point,
                                                     colour.withAlpha (0.0f), p.point.translated (glowR, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (glowR * 2.0f, glowR * 2.0f).withCentre (p.point));
        }

        juce::Path shape;
        switch (spk.type)
        {
            case SpeakerType::regular:
                shape.addRoundedRectangle (juce::Rectangle<float> (r * 1.6f, r * 2.0f).withCentre (p.point), r * 0.3f);
                break;
            case SpeakerType::sub:
                shape.addRoundedRectangle (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p.point), r * 0.25f);
                break;
            case SpeakerType::transducer:
                shape.startNewSubPath (p.point.translated (0.0f, -r * 1.1f));
                shape.lineTo (p.point.translated (r * 1.1f, 0.0f));
                shape.lineTo (p.point.translated (0.0f, r * 1.1f));
                shape.lineTo (p.point.translated (-r * 1.1f, 0.0f));
                shape.closeSubPath();
                break;
        }

        g.setColour (unreachable ? juce::Colours::darkgrey : colour.darker (0.9f - 0.8f * level));
        g.fillPath (shape);
        g.setColour (unreachable ? juce::Colours::grey : colour);
        g.strokePath (shape, juce::PathStrokeType (selected ? 2.5f : 1.3f));

        if (spk.type == SpeakerType::sub)
        {
            g.drawEllipse (juce::Rectangle<float> (r * 1.3f, r * 1.3f).withCentre (p.point), 1.2f);
        }

        if (selected)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (juce::Rectangle<float> (r * 3.2f, r * 3.2f).withCentre (p.point), 1.5f);
        }

        // Output number on the speaker, name under it when selected
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont (juce::FontOptions (juce::jlimit (9.0f, 15.0f, r * 0.9f), juce::Font::bold));
        g.drawText (juce::String (spk.output), juce::Rectangle<float> (r * 3.0f, r * 2.0f).withCentre (p.point),
                    juce::Justification::centred);

        if (selected || testing)
        {
            g.setFont (juce::FontOptions (12.0f));
            g.setColour (colours::text);
            g.drawText (spk.name + (unreachable ? "  (output not available)" : ""),
                        juce::Rectangle<float> (220.0f, 16.0f).withCentre (p.point.translated (0.0f, r * 1.6f + 10.0f)),
                        juce::Justification::centred);
        }
    }

    // Header and hints
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (room.name, 14, 12, getWidth() - 300, 20, juce::Justification::centredLeft);

    g.setColour (colours::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (room.width, 1) + " x " + juce::String (room.depth, 1) + " x " + juce::String (room.height, 1)
                    + " m   |   " + juce::String ((int) room.speakers.size()) + " speakers",
                14, 32, getWidth() - 300, 16, juce::Justification::centredLeft);

    g.drawText ("Drag to orbit  |  Scroll to zoom  |  Drag a speaker to move it  |  Shift-drag: height only  |  Click again: next speaker in a stack",
                14, getHeight() - 24, getWidth() - 28, 16, juce::Justification::centredLeft);

    // Legend
    auto legend = juce::Rectangle<int> (getWidth() - 300, getHeight() - 48, 290, 16);
    const std::pair<juce::Colour, juce::String> entries[] = {
        { colours::regular, "Regular" }, { colours::sub, "Sub" }, { colours::transducer, "Transducer" }, { colours::source, "Sound" } };
    for (const auto& [col, name] : entries)
    {
        auto slot = legend.removeFromLeft (72);
        g.setColour (col);
        g.fillEllipse (slot.removeFromLeft (12).toFloat().reduced (2.0f));
        g.setColour (colours::dimText);
        g.drawText (name, slot.withTrimmedLeft (4), juce::Justification::centredLeft);
    }
}

//==============================================================================
void RoomView3D::mouseDown (const juce::MouseEvent& e)
{
    lastMouse = e.position;
    mouseMovedSinceDown = false;
    selectedBeforeMouseDown = selectedSpeaker;
    hitsAtMouseDown = speakersAt (e.position);

    if (hitsAtMouseDown.empty())
    {
        dragMode = DragMode::orbit;
        return;
    }

    // Keep the selected speaker if it's under the mouse, otherwise take the nearest
    int hit = hitsAtMouseDown.front();
    if (std::find (hitsAtMouseDown.begin(), hitsAtMouseDown.end(), selectedSpeaker) != hitsAtMouseDown.end())
        hit = selectedSpeaker;

    selectedSpeaker = hit;
    draggedSpeaker = hit;
    dragStartHeight = room.speakers[(size_t) hit].position.z;
    dragMode = e.mods.isShiftDown() ? DragMode::raiseSpeaker : DragMode::moveSpeaker;

    if (onSpeakerSelected)
        onSpeakerSelected (hit);

    repaint();
}

void RoomView3D::mouseDrag (const juce::MouseEvent& e)
{
    const auto delta = e.position - lastMouse;
    lastMouse = e.position;

    if (e.getDistanceFromDragStart() > 2)
        mouseMovedSinceDown = true;

    if (dragMode == DragMode::orbit)
    {
        yawDegrees += delta.x * 0.4f;
        pitchDegrees = juce::jlimit (-5.0f, 89.5f, pitchDegrees + delta.y * 0.3f);
        repaint();
        return;
    }

    if (! mouseMovedSinceDown || draggedSpeaker < 0 || draggedSpeaker >= (int) room.speakers.size())
        return;

    auto& spk = room.speakers[(size_t) draggedSpeaker];
    auto snap = [] (float v) { return std::round (v * 20.0f) / 20.0f; }; // 5 cm steps
    const auto cam = makeCamera();

    if (dragMode == DragMode::raiseSpeaker || e.mods.isShiftDown())
    {
        // Height only
        const auto p = project (cam, spk.position);
        const float metresPerPixel = p.visible ? p.depth / cam.focal : 0.02f;
        spk.position.z = juce::jlimit (0.0f, room.height, snap (spk.position.z - delta.y * metresPerPixel));
    }
    else if (pitchDegrees < 30.0f)
    {
        // Looking from the front or side: move in the upright plane facing the camera
        const Vec3 normal = Vec3 { cam.forward.x, cam.forward.y, 0.0f }.normalised();
        Vec3 hit;
        if (rayHitsPlane (cam, e.position, spk.position, normal, hit))
        {
            spk.position.x = juce::jlimit (-room.width * 0.5f, room.width * 0.5f, snap (hit.x));
            spk.position.y = juce::jlimit (-room.depth * 0.5f, room.depth * 0.5f, snap (hit.y));
            spk.position.z = juce::jlimit (0.0f, room.height, snap (hit.z));
        }
    }
    else
    {
        // Looking from above: slide across the room at the same height
        Vec3 hit;
        if (rayHitsPlane (cam, e.position, spk.position, { 0.0f, 0.0f, 1.0f }, hit))
        {
            spk.position.x = juce::jlimit (-room.width * 0.5f, room.width * 0.5f, snap (hit.x));
            spk.position.y = juce::jlimit (-room.depth * 0.5f, room.depth * 0.5f, snap (hit.y));
        }
    }

    if (onSpeakerMoved)
        onSpeakerMoved (draggedSpeaker, spk.position);

    repaint();
}

void RoomView3D::mouseUp (const juce::MouseEvent&)
{
    const bool wasSpeaker = dragMode == DragMode::moveSpeaker || dragMode == DragMode::raiseSpeaker;
    dragMode = DragMode::none;
    draggedSpeaker = -1;

    if (! wasSpeaker)
        return;

    if (mouseMovedSinceDown)
    {
        if (onSpeakerDragEnded)
            onSpeakerDragEnded();
        return;
    }

    // A click without dragging on stacked speakers that was already selected:
    // step to the next one in the stack
    auto it = std::find (hitsAtMouseDown.begin(), hitsAtMouseDown.end(), selectedBeforeMouseDown);
    if (hitsAtMouseDown.size() > 1 && it != hitsAtMouseDown.end())
    {
        const auto current = (size_t) (it - hitsAtMouseDown.begin());
        selectedSpeaker = hitsAtMouseDown[(current + 1) % hitsAtMouseDown.size()];

        if (onSpeakerSelected)
            onSpeakerSelected (selectedSpeaker);

        repaint();
    }
}

void RoomView3D::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (speakersAt (e.position).empty())
        setCameraPreset (CameraPreset::perspective);
}

void RoomView3D::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    distance = juce::jlimit (2.0f, 200.0f, distance * (1.0f - wheel.deltaY * 0.5f));
    repaint();
}

void RoomView3D::mouseMagnify (const juce::MouseEvent&, float scaleFactor)
{
    distance = juce::jlimit (2.0f, 200.0f, distance / juce::jmax (0.1f, scaleFactor));
    repaint();
}

} // namespace spatial
