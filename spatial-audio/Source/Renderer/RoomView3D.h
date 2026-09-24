// Spatial Audio - the 3D "hologram" view of the room
//
// Shows the room, every speaker, the pathway and the moving sound.
//   Drag empty space      = orbit the camera
//   Scroll / pinch        = zoom
//   Double-click          = reset the camera
//   Click a speaker       = select it (click again to cycle through
//                           speakers stacked in the same spot)
//   Drag a speaker        = from above: move it across the room, keeping its height
//                           from the front or side: move it up/down and sideways
//   Shift + drag speaker  = raise or lower it only
#pragma once

#include "JuceIncludes.h"
#include "RoomLayout.h"
#include "AudioEngine.h"
#include "../Core/Pathway.h"

namespace spatial
{

class RoomView3D : public juce::Component,
                   private juce::Timer
{
public:
    explicit RoomView3D (AudioEngine& engine);

    void setRoom (const RoomLayout& room);
    void setPath (const PathParams& path);
    void setSelectedSpeaker (int index);

    enum class CameraPreset { perspective, top, front, side };
    void setCameraPreset (CameraPreset preset);

    std::function<void (int index)> onSpeakerSelected;
    std::function<void (int index, Vec3 newPosition)> onSpeakerMoved;
    std::function<void()> onSpeakerDragEnded;

    // juce::Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

private:
    struct Camera
    {
        Vec3 eye, forward, right, up;
        float focal = 500.0f;
        juce::Point<float> centre;
    };

    struct Projected
    {
        juce::Point<float> point;
        float depth = 0.0f;
        bool visible = false;
    };

    void timerCallback() override;
    Camera makeCamera() const;
    Projected project (const Camera& cam, const Vec3& p) const;
    bool rayHitsPlane (const Camera& cam, juce::Point<float> screen, const Vec3& planePoint,
                       const Vec3& planeNormal, Vec3& hit) const;
    std::vector<int> speakersAt (juce::Point<float> screen) const; // nearest first
    void drawLine3D (juce::Graphics& g, const Camera& cam, const Vec3& a, const Vec3& b) const;

    AudioEngine& engine;
    RoomLayout room;
    PathParams path;
    int selectedSpeaker = -1;

    float yawDegrees = -25.0f;
    float pitchDegrees = 35.0f;
    float distance = 14.0f;

    enum class DragMode { none, orbit, moveSpeaker, raiseSpeaker };
    DragMode dragMode = DragMode::none;
    juce::Point<float> lastMouse;
    int draggedSpeaker = -1;
    float dragStartHeight = 0.0f;
    bool mouseMovedSinceDown = false;
    std::vector<int> hitsAtMouseDown;
    int selectedBeforeMouseDown = -1;

    juce::TextButton perspectiveButton { "3D" }, topButton { "Top" }, frontButton { "Front" }, sideButton { "Side" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomView3D)
};

// Colours shared with the rest of the interface
namespace colours
{
    const juce::Colour background   { 0xff0b0d12 };
    const juce::Colour panel        { 0xff151923 };
    const juce::Colour grid         { 0xff1f2633 };
    const juce::Colour roomEdge     { 0xff3a4a66 };
    const juce::Colour regular      { 0xff4fd1c5 };
    const juce::Colour sub          { 0xffff9f43 };
    const juce::Colour transducer   { 0xffb388ff };
    const juce::Colour source       { 0xffff5c8a };
    const juce::Colour pathLine     { 0xffff5c8a };
    const juce::Colour text         { 0xffd8dee9 };
    const juce::Colour dimText      { 0xff7a869a };

    inline juce::Colour forSpeaker (SpeakerType t)
    {
        switch (t)
        {
            case SpeakerType::sub:        return sub;
            case SpeakerType::transducer: return transducer;
            case SpeakerType::regular:    break;
        }
        return regular;
    }
}

} // namespace spatial
