# Spatial Audio

A free, open source spatial audio system for Ableton Live. Place and move sounds through any speaker setup, from 4 speakers to 24 or more, and see the whole composition in a 3D view.

Made by [Ruby Singh](https://rubysingh.ca). Mac first.

## What's here now (version 0.1)

**Spatial Renderer**, a Mac app that:

- Loads, edits and saves **room layouts**: add, remove and move speakers, by typing positions or dragging them in the 3D view
- Supports three **speaker types**: regular speakers, subs, and vibroacoustic transducers
- Pans sound across the speakers with **DBAP** (distance-based amplitude panning), so several speakers work together to place a sound anywhere in the room, even with irregular layouts
- Moves a test sound along a **pathway**: line, circle, triangle, square, figure eight or spiral, forward, reverse or back and forth, with free speed or tempo lock
- Shows it all in a **3D view** you can orbit and zoom

Coming next: the Ableton Live plugin, so each track in Live becomes a source you can place and automate.

## Getting the app

1. Open the **Actions** tab of this repository.
2. Click the most recent green run of **Build Mac app**.
3. Under **Artifacts**, download **Spatial-Renderer-mac**.
4. Unzip it (you may need to unzip twice) and drag **Spatial Renderer** into Applications.
5. The first time you open it, macOS will block it because it isn't from the App Store. Go to **System Settings > Privacy & Security**, scroll down, and click **Open Anyway**.

## First test

1. Plug in your audio interface.
2. Click **Audio settings...**, choose your interface as the output, and make sure all outputs are ticked.
3. In the **Room** tab, load a preset or build your own layout. Set each speaker's **Output** to the interface output it's plugged into.
4. Keep the **Master level** low, then click **Test each speaker**. A burst plays through each speaker in turn, and the matching speaker lights up in the 3D view. Check they match.
5. Click **Play test sound** and watch (and listen to) the sound travel along the pathway.

## Room files

Rooms are saved as simple text files (`.json`). Positions are in metres:

- `x`: left (negative) to right (positive), from the centre of the room
- `y`: back (negative) to front (positive), from the centre of the room
- `z`: height above the floor
- `type`: `regular`, `sub` or `transducer`
- `output`: the interface output number, starting at 1
- `trim_db`: level adjustment for that speaker

See `Rooms/example-studio.json`.

## How movement stays smooth

Neighbouring speakers always share the sound, and the balance between them changes continuously as it moves (recalculated about every 1.3 ms). In the **Sound** tab:

- **Spread**: how many neighbouring speakers share the sound. More spread gives softer, smoother movement.
- **Low spread**: frequencies below 300 Hz are shared even more widely, so bass flows through the room while the highs stay more precise.
- **Focus**: how strongly the nearest speakers take over.
- **Glide**: eases sudden jumps in position.

The **%** button in the 3D view shows how much of the sound each speaker is playing.

## How the sound is split between speaker types

- **Regular** speakers get the full sound, panned by distance.
- **Subs** get everything below the sub crossover (80 Hz by default). With several subs, the bass leans toward the ones nearest the sound.
- **Transducers** get everything below their range setting (200 Hz by default) and follow the sound among the transducers, so vibration moves with it.

## Building it yourself

You don't need to: GitHub builds the app automatically on every change. If you want to, you need CMake 3.22+ and Xcode:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## License

GNU Affero General Public License v3.0. See `LICENSE`. Built with [JUCE](https://juce.com).
