# Scene 3DS benchmark

How many triangles a 3DS's GPU (the PICA200) draws a frame, measured on the hardware, for the scene layer's 3DS budgets. The DS's limits are fixed (2048 polygons and 6144 vertices a frame, the rest dropped); the 3DS's GPU has no cap, only how fast it draws, and the figures in circulation are unsourced (see [`DESIGN.md`](../DESIGN.md)). This app draws with [citro3d](https://github.com/devkitPro/citro3d) directly, so its numbers come before the 3DS backend and shape it.

```bash
python3 tools/fbi_install/fbi_install.py --3ds ADDRESS //projects/scratchpadd/scene/3ds_benchmark:scene-3ds-benchmark
```

(or `bazel run` it in Azahar, which runs it but doesn't model the GPU's timing: there, every case is the CPU's.) It runs by itself for a minute or two, drawing each case on the top screen, and shows the results on the bottom one; they're also written to `sdmc:/scene-3ds-benchmark.txt` with every measurement (fetch it with ftpd: see [`tools/fbi_install`](../../../../tools/fbi_install/README.md)). START stops it.

**How it measures.** Each case draws objects of 128 triangles (an 8 x 8 grid of quads, from one vertex buffer of position, normal and colour), each with its own transform, as a scene draws meshes: a uniform upload and a draw each. It doubles the objects from 1 until a frame takes over 40 ms, taking each frame's GPU time (`C3D_GetDrawingTime`) and CPU time (`C3D_GetProcessingTime`) over 12 frames after 3 to warm up. The triangles that fit in 16.7 ms (60 fps) and 33.3 ms (30 fps) are interpolated between the doublings either side, from whichever of the GPU and CPU is slower, and the results say which it was.

**The cases**, every combination of:
- **Unlit or lit:** vertex colours only, or the GPU's fragment lighting with ambient and 4 directional lights (the scene layer's most), times the vertex colours.
- **Small or large triangles:** objects 24 pixels across (about 4 pixels a triangle: vertex-bound) or 240 (about 450, overlapping: fill-bound). The projection is orthographic, so sizes are exact.
- **Mono or stereo:** stereo draws the top screen once for each eye (whatever the 3D slider says).
- **268 or 804 MHz:** on a New 3DS, the same again with its fast CPU mode (`osSetSpeedupEnable`), which shows whether submitting draws is what limits (the GPU's clock doesn't change).

## Results: a New 3DS XL (2026-11-12)

Triangles a frame (the GPU was the limit in every case; at 804 MHz the figures were within 2% of these, 268 MHz's):

| Case | 60 fps | 30 fps |
| --- | --- | --- |
| Unlit, small, mono | 105k | 216k |
| Unlit, small, stereo | 50k | 106k |
| Unlit, large, mono | 7.5k | 15.4k |
| Unlit, large, stereo | 3.8k | 7.6k |
| Lit, small, mono | 95k | 195k |
| Lit, small, stereo | 45k | 95k |
| Lit, large, mono | 3.8k | 7.2k |
| Lit, large, stereo | 1.9k | 3.8k |

- **Geometry is plentiful:** about 95,000 lit triangles a frame at 60 fps, a few pixels each, against the 10-30k guessed in the research (unsourced). Non-indexed triangles, so meshes sharing vertices may do better.
- **Pixels are the limit:** about 3,800 large lit triangles a frame at 60 fps, very roughly 1.7 million pixels (many objects reach off the screen), about 18 times the top screen's area. Fragment lighting halves it, being worked out for each pixel.
- **Stereo halves everything,** as the research expected.
- **Each draw costs the CPU about 15 µs at 268 MHz** (5 at 804, the New 3DS's fast mode), so 1,000 draws take 15 ms of a 60 fps frame: the number of draws needs a budget of its own. Fast mode doesn't change the GPU's limits.
- **A frame costs about 0.8 ms before anything is drawn** (1.5 in stereo): clearing and copying out the screens.
- An Old 3DS's GPU runs at the same 268 MHz, so its GPU figures should match; its CPU is like these 268 MHz runs but without the New 3DS's L2 cache, so its draws may cost more. Not measured.
