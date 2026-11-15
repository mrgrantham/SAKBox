# Network: an app's networking, in one padd

An app's networking happens in **`NetworkPadd`**. It starts the network, then runs every **service** registered with it in its **I/O loop**. That's one thread waiting on every socket at once, so no padd ever waits on the network. A service only says where it listens or connects, and reacts to its connections' bytes; the loop does the accepting, receiving and sending.

```cpp
#include <ScratchPadd/Network/NetworkPadd.hpp>         // //projects/scratchpadd/network:network_padd
#include <ScratchPadd/Network/RemoteControlsPadd.hpp>

ScratchPadd::SystemImplementation<..., ScratchPadd::Network::NetworkPadd,
                                  ScratchPadd::Network::RemoteControlsPadd> system;
```

What uses it now:

| What | Port | Service | Read with |
| --- | --- | --- | --- |
| A 3DS app's debug log | 5005 | `StreamService`, registered by the log's `Session` | `tools/console_log --3ds ADDRESS`, or `nc ADDRESS 5005` |
| An app's controls from another device | 5006 | `LineService`, in `RemoteControlsPadd` | [`remote_controls`](../remote_controls/README.md) on the Mac |
| Messages between devices (the bridge) | 5007 | `FrameService`, in an app's `BridgePadd` | the same app on the other devices |

The ScratchPadd 3DS demo and the Cyclone 3DS ragdoll list both padds. A DS has no network here (a DS Lite can't join a WPA2 network): its apps don't list them, and `NetworkPadd` there would only say so.

## The layers

It's built in layers, so the uses still to come fit (see the [roadmap](#roadmap)):

1. **Sockets** (`Sockets.hpp`): TCP and UDP the same on every platform. That's POSIX on macOS and Linux, Winsock on Windows, and soc on the 3DS, which `Network::start()` gives its own buffer. `deviceAddress()` gives the address others reach this device on.
2. **The loop** (`Loop`): one thread waiting (`poll`, or `WSAPoll` on Windows) on:
   - each service's listening socket or outgoing connection;
   - every connection;
   - a **wake socket**, a UDP socket a service pokes from any other thread when it has something new to send. It's on 127.0.0.1 where there's a loopback. A real 3DS has none (binding 127.0.0.1 there is "Invalid argument", which Azahar, on the Mac's network, didn't show), so on a 3DS it's on the device's own address, on the first free port from 5100. Where none can be made at all, the loop still runs, looking for work every 50 ms instead of being woken, and NetworkPadd's tab says so ("Woken: every 50 ms").

   Waking that way, the loop never polls on a timer for work. It still wakes once a second, to retry outgoing connections and update its figures. An outgoing connection is started without waiting and waited for with the other sockets, so the loop never stalls on one. Windows takes 2 s to give up on a refused connection, and an unreachable address takes longer anywhere. One that fails (the other side isn't there yet) is tried again 2 s after it fails. `detach()` returns once the loop has let go of a service, so the service can then be destroyed. The loop is plain C++, tested without padds.
3. **Services** (`Service.hpp`) are what an app does over the network. Each says where it listens or connects (`endpoint()`) and reacts on the loop's thread:
   - `connected()` and `disconnected()`;
   - `received()`, for each connection's bytes;
   - `pump()`, each pass, to queue anything new.

   From another thread, a service calls `wake()` to have its `pump()` run. Services made outside the padds (the log's, in `main`) are registered in `services()`, where `NetworkPadd` finds them, including any registered while it runs.
   - **`StreamService`**: text served to whoever connects. It sends what's been kept, then the rest as it comes, to each reader from where that reader is (several at once). Its text comes from a function, so any growing text can be served: the log's is a `DebugLog::LogBuffer`, which calls the service's `wake()` on each append.
   - **`LineService`**: lines of text both ways, listening or connecting. It hands each line received to a handler, and `send()` queues a line from any thread to one connection or to all.
4. **Padds:**
   - **`NetworkPadd`** runs the loop. Its tab shows the device's address and, for each service, where it listens or connects, what it's doing, how many are connected, and what it has sent and received (Info).
   - **`RemoteControlsPadd`** carries control messages between the network and the padds. Whoever connects to port 5006 gets every padd's controls, then each change after. A change sent from there goes to the padd it's for, as a panel's does. Messages are [`ControlsText`](../messages/include/ScratchPadd/Messages/ControlsText.hpp) lines: a request, a snapshot or a change, with tab-separated fields and text escaped. It only makes text while someone's connected, because formatting numbers costs a console time.

## The message bridge

`BridgePadd` carries chosen ScratchPadd messages between devices. An app subclasses it and lists the types that cross. When one of the app's padds sends one, the bridge sends it to the other devices, and each of them sends it to its own padds, as if it had been sent there:

```cpp
struct Move { std::int32_t x, y; };                 // trivially copyable: crosses byte for byte
class GameBridge : public Network::BridgePadd {
public:
  explicit GameBridge(System *system) : BridgePadd(system) {
    carry<Move>("move");                            // the same name on every device
    carry<Chat>("chat", encodeChat, decodeChat);    // anything else: its own encoding
  }
};
ScratchPadd::SystemImplementation<..., NetworkPadd, GameBridge> system;

send(Network::BridgeHost{});                                  // on one device: it makes a join code
send(Network::BridgeJoin{.address = "192.168.4.45", .code = "K7QM"});  // on the others; BridgeLeave{} to stop
```

**Seats** say who sent a message, and who to send one to:

```cpp
receive<Network::From<Move>>([this](const Network::From<Move> &move) {
  apply(move.seat, move.message);                             // any Move, and the seat it came from
});
send(Network::To<Hand>{.seat = 3, .message = hand});         // a Hand for seat 3 only
const Network::BridgeState bridge = currentState<Network::BridgeState>().value;
bridge.seat;  bridge.taken(2);  bridge.players();             // this device's seat, and who's in
```

- **One device hosts and the others join it** (a star, on port 5007). The host passes each joiner's messages on to the others, so every device gets every carried message, and none gets its own back.
- **Seats:** the host gives each device a seat. The host is seat 1, and the others get 2 to 16 in the order they join. A device that drops out and comes back asks for its old seat, and gets it if it's still free.
  - **Who sent it:** every carried message also arrives as `From<T>`, with the seat it came from. The sender gets `From<T>` for its own messages too, so a host's rules padd hears every player the same way, its own player included. Off a game, the seat is `kNoSeat`.
  - **Send to one:** `To<T>` sends a carried message to one seat only: private cards, or one player's turn. From a joiner to another joiner, it goes through the host. A device's own seat sends it to its own padds.
  - **Who's in:** `BridgeState` has this device's seat and the seats taken. It's published as shared state, and it's also sent as a message whenever it changes, so a padd can react to a player joining or leaving.
  - **The host decides seats:** it reads each message's sender from the connection it arrived on, not from what the message says, so a device can't pose as another seat.
- **Join code:** when hosting, the bridge makes a four-character code from letters and digits that can't be misread (no O and 0, no I and 1). It shows the code on its tab, and refuses any device that doesn't bring it, telling that device why ("wrong code", "the game is full", "another version of the bridge"). `BridgeHost{.code = "..."}` sets the code; `BridgeHost{.code = ""}` lets anyone join (tests, or a trusted network). The code decides who joins; the traffic itself isn't encrypted.
- **How it hooks in:** the bridge handles each carried type like any other padd. A padd's `send()` skips the sender, so when the bridge re-sends a message that arrived, it doesn't receive it again, and nothing loops.
- **Byte for byte:** a trivially copyable message must be laid out the same on every device. That means fixed-width types (`std::int32_t`, `float`) and no pointers or `std::size_t`, which are 4 bytes on a console and 8 on a Mac. Its size travels with it, and one of another size is dropped with a warning, rather than read wrongly. Anything else (strings, vectors) takes an encode and a decode function.
- **On the wire:** a `FrameService`. Frames are length-prefixed binary, and each starts with a kind byte:
  - a message: its sender's seat, the seat it's for (or all), the type's name, then its bytes;
  - a joiner's hello: the protocol version, the seat it had, and the code;
  - the host's answers: a welcome with the joiner's seat, or a refusal with the reason (after which the host closes the connection);
  - the seats taken, sent by the host whenever they change.

  It runs over TCP, so every message arrives, in order.
- **Its tab:** what it's doing ("hosting on port 5007, code K7QM, 3 seated", "seat 2 of 3", or "refused: wrong code"). The Mode, Address and Code controls host and join from a panel, so `remote_controls` can link two consoles; a host shows its code in Code.
- Checked by `Bridge_test`:
  - three systems linked on localhost: messages from the host to both joiners, and from a joiner to the host and on to the other;
  - a message with its own encoding, and one of another size dropped;
  - seats given out, and the same on every device;
  - `From<T>` with the sender's seat, the sender's own included;
  - `To<T>` from the host, to the host, and from one joiner to another;
  - a wrong code refused;
  - a device that leaves and rejoins getting its seat back.

  It ran 10 times normally and 10 times under ThreadSanitizer. Not yet on consoles.

### Decisions

Settled on the roadmap page (2026-11-14):

1. **Topology:** a star through the host. The host seeing all traffic is fine for games among friends.
2. **Names:** `BridgePadd` and `carry<T>()`, kept: "share" would be confused with ScratchPadd's shared state.
3. **Who sent it, and sending to one:** seats, `From<T>` and `To<T>` (above).
4. **Joining securely:** a join code that the host shows and joiners type. Typing works everywhere, since the DS Lite has no camera; QR codes could be an extra on the 3DS later.

Still to build, in this order (see the roadmap below): finding hosts by broadcast, then UDP "latest wins" delivery for messages sent many times a second (`carry<Move>("move", Delivery::Latest)`), then the DS's local wireless as a transport under the same bridge.

## Roadmap

These are the next branches, each built on the layers above:

1. **Finding hosts by broadcast:** a host announces itself by UDP, and joiners list the hosts they hear (a "join game" screen), so nobody types an address.
2. **UDP "latest wins" delivery,** for messages sent many times a second, where a lost one is simply replaced by the next (the ship game's ghosts).
3. **Local wireless on the DS (NiFi):** consoles together with no router, as another transport under the bridge, with a game not caring which carries it. It stays on devkitPro: our own ARM7 program and an ARM9 library, building on calico's wireless driver, whose local wireless mode is still empty ([the plan](../../../docs/research/hex-trading-game.md#decided-so-far-from-the-roadmap-page-2026-11-14)). The 3DS's own `uds` comes later.

## Parts

| Target | What |
| --- | --- |
| `:network` | `Sockets.hpp`, `Loop`, `Service` and `services()`, `StreamService`, `LineService`, `FrameService`. Plain C++, no padds |
| `:network_padd` | `NetworkPadd`, `RemoteControlsPadd` and `BridgePadd` |
| `:sources` | Everything, for the 3DS's Makefile builds (`FOLDER/src` in `SOURCES`, `FOLDER/include` in `INCLUDES`) |

| Test | Covers |
| --- | --- |
| `Loop_test` | On localhost, without padds: a stream served to readers from the start and as it grows, two readers at once, and letting go of it closing its connections; lines both ways between two loops, a connecting service waiting for its listener and connecting on its next try |
| `Bridge_test` | Three systems linked on localhost (one NetworkPadd running the network for all): carried messages from the host to those that joined, from one that joined to the host and on to the other, never back to the sender; a message with its own encoding; one of another size dropped; seats given out and seen the same everywhere; `From<T>` with the sender's seat; `To<T>` to one seat, through the host between joiners; a wrong join code refused; a returning device getting its seat back |
| `RemoteControls_test` | In a running system, with a client on localhost: connecting bringing every padd's controls, a change sent from afar made by its padd and its new snapshot sent back, and NetworkPadd's tab showing the service and its client |
| `messages:ControlsText_test` | Every kind of control through a snapshot line and back exactly (ranges, choices, text with tabs, newlines and backslashes); a change and a request; lines that aren't messages refused |

`Loop_test` and `RemoteControls_test` also ran 10 times each under ThreadSanitizer. Checked in Azahar, whose network is the Mac's: `console_log --3ds 127.0.0.1` read the 3DS demo's log from its first line, and `remote_controls` showed its padds' controls.
