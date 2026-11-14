# Remote controls: a console's controls on the Mac

`remote_controls` connects to an app's `RemoteControlsPadd` ([network](../network/README.md)) and shows every padd's controls there in the Controls window, as if they were a desktop app's own: the sliders, checkboxes, choices, and information such as the About tab's table. A change made here goes to the padd on the console, and the console's answer, its new snapshot, comes back, so the window always shows what the console really holds.

```bash
bazel run //projects/scratchpadd/remote_controls -- 192.168.4.25    # a 3DS: its address (NetworkPadd's tab shows it)
bazel run //projects/scratchpadd/remote_controls                    # Azahar, on this Mac (127.0.0.1)
bazel run //projects/scratchpadd/remote_controls -- HOST PORT       # another port than 5006
```

Until it's connected (the app not started yet, or gone), it tries again every 2 s and says so at the top of the window. When the console's app goes, the tabs go with it, and they come back when it returns.

The console app needs a `NetworkPadd` and a `RemoteControlsPadd`, as the ScratchPadd 3DS demo and the Cyclone 3DS ragdoll have. A DS can't be reached this way: a DS Lite can't join a WPA2 network.

How it works: a `RemoteWindowPadd` draws the window and keeps a `LineService` connected to the console. `NetworkPadd` runs it here too, the same padd the console runs. Lines from the console are [`ControlsText`](../messages/include/ScratchPadd/Messages/ControlsText.hpp) snapshots, shown with `DebugUi::ControlPanels`. A change made in the window is sent back as a `ControlsText` change, by the padd's name.

Checked against the 3DS demo in Azahar: its Audio, Network, Storage and About tabs, connected. Changing a control from afar is checked by `network:RemoteControls_test`.
