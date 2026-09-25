#include "RoomLayout.h"

namespace spatial
{

juce::String speakerTypeToString (SpeakerType t)
{
    switch (t)
    {
        case SpeakerType::sub:        return "sub";
        case SpeakerType::transducer: return "transducer";
        case SpeakerType::regular:    break;
    }
    return "regular";
}

juce::String speakerTypeDisplayName (SpeakerType t)
{
    switch (t)
    {
        case SpeakerType::sub:        return "Sub";
        case SpeakerType::transducer: return "Transducer";
        case SpeakerType::regular:    break;
    }
    return "Regular";
}

SpeakerType speakerTypeFromString (const juce::String& s)
{
    const auto lower = s.trim().toLowerCase();
    if (lower == "sub" || lower == "subwoofer")                         return SpeakerType::sub;
    if (lower.startsWith ("transducer") || lower.startsWith ("vibro")) return SpeakerType::transducer;
    return SpeakerType::regular;
}

static double roundTo (double v, double step) { return std::round (v / step) * step; }

juce::String RoomLayout::toJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", name);

    auto* size = new juce::DynamicObject();
    size->setProperty ("width", roundTo (width, 0.01));
    size->setProperty ("depth", roundTo (depth, 0.01));
    size->setProperty ("height", roundTo (height, 0.01));
    root->setProperty ("size", juce::var (size));

    juce::Array<juce::var> list;
    for (const auto& s : speakers)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", s.name);
        o->setProperty ("type", speakerTypeToString (s.type));
        o->setProperty ("x", roundTo (s.position.x, 0.01));
        o->setProperty ("y", roundTo (s.position.y, 0.01));
        o->setProperty ("z", roundTo (s.position.z, 0.01));
        o->setProperty ("output", s.output);
        o->setProperty ("trim_db", roundTo (s.trimDb, 0.1));
        list.add (juce::var (o));
    }
    root->setProperty ("speakers", list);

    return juce::JSON::toString (juce::var (root));
}

bool RoomLayout::fromJson (const juce::String& json, RoomLayout& result, juce::String& errorMessage)
{
    juce::var parsed;
    const auto parseResult = juce::JSON::parse (json, parsed);

    if (parseResult.failed())
    {
        errorMessage = "The room file couldn't be read: " + parseResult.getErrorMessage();
        return false;
    }

    if (! parsed.isObject())
    {
        errorMessage = "The room file should start with { and end with }.";
        return false;
    }

    RoomLayout room;
    room.name = parsed.getProperty ("name", "Untitled room").toString();

    const auto size = parsed.getProperty ("size", {});
    if (size.isObject())
    {
        room.width  = (float) (double) size.getProperty ("width", 6.0);
        room.depth  = (float) (double) size.getProperty ("depth", 6.0);
        room.height = (float) (double) size.getProperty ("height", 3.0);
    }

    room.width  = juce::jlimit (0.5f, 200.0f, room.width);
    room.depth  = juce::jlimit (0.5f, 200.0f, room.depth);
    room.height = juce::jlimit (0.5f, 100.0f, room.height);

    const auto speakers = parsed.getProperty ("speakers", {});
    if (! speakers.isArray())
    {
        errorMessage = "The room file needs a \"speakers\" list.";
        return false;
    }

    int index = 0;
    for (const auto& s : *speakers.getArray())
    {
        ++index;
        if (! s.isObject())
        {
            errorMessage = "Speaker " + juce::String (index) + " isn't written correctly.";
            return false;
        }

        Speaker sp;
        sp.name = s.getProperty ("name", "Speaker " + juce::String (index)).toString();
        sp.type = speakerTypeFromString (s.getProperty ("type", "regular").toString());
        sp.position = { (float) (double) s.getProperty ("x", 0.0),
                        (float) (double) s.getProperty ("y", 0.0),
                        (float) (double) s.getProperty ("z", 1.2) };
        sp.output = (int) s.getProperty ("output", index);
        sp.trimDb = (float) (double) s.getProperty ("trim_db", 0.0);

        if (sp.output < 1 || sp.output > 256)
        {
            errorMessage = "Speaker \"" + sp.name + "\" has output " + juce::String (sp.output)
                         + ". Outputs start at 1.";
            return false;
        }

        room.speakers.push_back (sp);
    }

    result = std::move (room);
    return true;
}

int RoomLayout::highestOutput() const
{
    int highest = 0;
    for (const auto& s : speakers)
        highest = juce::jmax (highest, s.output);
    return highest;
}

int RoomLayout::nextFreeOutput() const
{
    for (int out = 1; out <= 256; ++out)
    {
        bool used = false;
        for (const auto& s : speakers)
            used = used || s.output == out;
        if (! used)
            return out;
    }
    return highestOutput() + 1;
}

//==============================================================================
static void addRing (RoomLayout& room, int count, float radius, float height,
                     float startAngleDegrees, const juce::String& prefix, int& output)
{
    for (int i = 0; i < count; ++i)
    {
        // Angle measured clockwise from straight ahead.
        const float deg = startAngleDegrees + 360.0f * (float) i / (float) count;
        const float rad = degreesToRadians (deg);
        Speaker s;
        s.name = prefix + " " + juce::String (i + 1);
        s.position = { radius * std::sin (rad), radius * std::cos (rad), height };
        s.output = output++;
        room.speakers.push_back (s);
    }
}

std::vector<RoomLayout> RoomLayout::builtInPresets()
{
    std::vector<RoomLayout> presets;

    {
        RoomLayout r;
        r.name = "Quad (4 speakers)";
        r.width = 5.0f; r.depth = 5.0f; r.height = 3.0f;
        int out = 1;
        addRing (r, 4, 2.2f, 1.2f, -45.0f, "Speaker", out);
        presets.push_back (r);
    }
    {
        RoomLayout r;
        r.name = "Ring of 8";
        r.width = 7.0f; r.depth = 7.0f; r.height = 3.5f;
        int out = 1;
        addRing (r, 8, 3.0f, 1.2f, -22.5f, "Ring", out);
        presets.push_back (r);
    }
    {
        RoomLayout r;
        r.name = "8 + 4 height + sub (13)";
        r.width = 8.0f; r.depth = 8.0f; r.height = 4.0f;
        int out = 1;
        addRing (r, 8, 3.2f, 1.2f, -22.5f, "Ring", out);
        addRing (r, 4, 2.2f, 3.4f, -45.0f, "Height", out);
        Speaker sub;
        sub.name = "Sub"; sub.type = SpeakerType::sub;
        sub.position = { 0.0f, 3.5f, 0.2f };
        sub.output = out++;
        r.speakers.push_back (sub);
        presets.push_back (r);
    }
    {
        RoomLayout r;
        r.name = "16 + 6 height + 2 subs (24)";
        r.width = 10.0f; r.depth = 10.0f; r.height = 5.0f;
        int out = 1;
        addRing (r, 16, 4.2f, 1.2f, -11.25f, "Ring", out);
        addRing (r, 6, 3.0f, 4.2f, 0.0f, "Height", out);
        for (int i = 0; i < 2; ++i)
        {
            Speaker sub;
            sub.name = "Sub " + juce::String (i + 1);
            sub.type = SpeakerType::sub;
            sub.position = { i == 0 ? -3.0f : 3.0f, 0.0f, 0.2f };
            sub.output = out++;
            r.speakers.push_back (sub);
        }
        presets.push_back (r);
    }
    {
        RoomLayout r;
        r.name = "Quad + 4 floor transducers (8)";
        r.width = 5.0f; r.depth = 5.0f; r.height = 3.0f;
        int out = 1;
        addRing (r, 4, 2.2f, 1.2f, -45.0f, "Speaker", out);
        const Vec3 tx[4] = { { -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f } };
        for (int i = 0; i < 4; ++i)
        {
            Speaker s;
            s.name = "Floor " + juce::String (i + 1);
            s.type = SpeakerType::transducer;
            s.position = tx[i];
            s.output = out++;
            r.speakers.push_back (s);
        }
        presets.push_back (r);
    }

    return presets;
}

} // namespace spatial
