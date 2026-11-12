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
