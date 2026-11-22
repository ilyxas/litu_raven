# Bluetooth Serial Connect: Raven setup

The ESP advertises **Raven-BLE**. It implements the app's default HM-10-compatible
profile directly; no UART Bluetooth module or changes to the Raven iOS project
are needed. This firmware controls two simulated arms, not actuator GPIOs.

Authoritative app references: [hardware and UUIDs](https://bluetooth.henhen1227.com/hardware.html),
[ASCII wire format](https://bluetooth.henhen1227.com/wire-protocol.html).

## Connect

1. In Bluetooth Serial Connect, disconnect the **Mock Board Simulator**.
2. Scan for and connect to **Raven-BLE**. Device Center defaults are service
   `0000FFE0-0000-1000-8000-00805F9B34FB`, read/write characteristic
   `0000FFE1-0000-1000-8000-00805F9B34FB`.
3. Notifications must be enabled on FFE1 before sending commands. The characteristic
   supports Read, Write, Write Without Response and Notify. One central at a time;
   disconnecting resumes advertising. Development mode uses no pairing/passkey.
4. Open **Console**, type `help`. The app wraps this as `C:help\n` automatically.
   A generic BLE terminal may send raw `help\n`; always include a real newline.

Replies use `D0:...\n`, visible in the console and in Text Display channel 0.
For a quiet console, send **`watch off`**: command replies and status queries still
work, but automatic widget updates stop. `watch on` restores a fresh snapshot.
This setting belongs to the connection; reconnecting enables readouts again.
For example:

```text
move left 45
move right -30
status left
pair 60 -60
status activity
halt
```

`D0:ACK id=... accepted=... rejected=... reason=0` confirms delivery to inboxes.
It does not mean the operation has run. `status left` reads the service's current
operation; `status left 1610612736` reads that specific retained result. A query
returns `STATUS`, without an additional ACK. An ID evicted from history is Unknown.

Other console commands:

| Command | Effect |
|---|---|
| `watch off` / `watch on` | Disable / enable automatic board readouts |
| `control left 0` | Establish/reuse this connection's continuous control |
| `target left 90` | Update that control's target; NACK if no active session |
| `halt left`, `halt right`, `halt all` | Cut selected owners; `all` includes Activity |
| `mode left callback` | Simulated callback completion |
| `mode left silent` or `stalled` | Exercise service-owned timeout |
| `mode left error` | Simulate device failure |
| `mode left polling` | Restore normal polling driver |

Targets are degrees in `-180..180`. Endpoints are `left`, `right`, `activity`
(Activity supports status; Pair has its own command). The adapter allocates IDs
in `0x60000000..0x7fffffff`, separate from binary callers and Activity children.
IDs continue across reconnects within one boot. Control target ACK IDs are delivery
correlation IDs; `status left` reports the continuing Control's operation ID.

## Board bindings

Channels belong to each widget type: B0, S0, J0 and D0 are different bindings.
These are **demo bindings**, not universal architecture rules. Edit them in
`SerialControlProtocol::handle()` when the application changes.

| Widget/channel | Bindings |
|---|---|
| Button B0 | Press/repeat = HALT both arms and Activity; release has no action |
| Button B1 | Press = Pair `45, -45` |
| Button B2 | Press = Pair `0, 0` |
| Slider S0 / S1 | Finite Move left / right; configure range `-180..180` |
| Joystick J0 / J1 | Continuous left / right; target = `90 × magnitude × cos(angle)` |
| Toggle T0 / T1 | On starts continuous control at current position; off cuts that arm |
| D-Pad P0 / P1 | U/R = +90, D/L = -90, C = 0; press/repeat controls, release cuts |
| Text Display D0 | Last ACK, NACK, help or requested status |
| Text Display D1 / D2 / D3 | Left / right / Activity status and operation ID |
| Gauge G0 / G1 | Left / right position; configure range `-180..180` |
| Gauge G2 | Simulated temperature; e.g. range `20..30` |
| Indicator I0 / I1 | Left / right operation Running |
| Indicator I3 | Connected readout; firmware sends 1 while notifications are active |
| Label L0 | `Raven ready` |

Joystick recenter sends magnitude zero: the demo commands a return to 0 degrees,
and the control remains Running. Use B0 or toggle off to halt. A later fresh
control input can begin a new session; HALT is not a permanent lockout.
B1/B2 releases/repeats are acknowledged as no-ops. No-op replies have no operation ID.
The on-device I3 value cannot clear itself over a disconnected link; use the app's
connection indicator to detect disconnects.

Input examples (literal wire lines, terminated by LF):

```text
B0:D
S0:45
J1:180,0.500
T0:1
P1:U,D
C:status right
```

## Transport and configuration

`BleSerialServer` owns GATT and bounded queues: eight RX packets of up to 512
bytes, sixteen TX frames of up to 192 bytes. The gateway parses at most eight RX
packets per turn; it never waits for an arm operation. NimBLE sends notification
fragments according to the negotiated ATT MTU. The preferred MTU is 185; MTU 23
also works. Complete messages use LF, independent of ATT packet boundaries.

Input lines are limited to 127 bytes, excluding LF. CRLF is accepted. Invalid,
oversized or non-ASCII input is discarded through the next LF and returns NACK.
Console commands use one line; escaped multiline commands are rejected.
An RX overflow or inability to queue a reply ends the BLE connection, preventing
parsing across lost bytes. A stalled outgoing fragment has a 500 ms retry budget.
As with TCP, losing a reply does not undo an admitted operation: reconnect and poll.

Readouts are sampled at most twice a second when the TX queue is empty; only
changed formatted values are sent, so pending replies take precedence. `watch off`
suppresses unsolicited output, including simulated temperature. The initial
snapshot and a new `watch on` refresh all widgets. Partial input, queued output and control bindings are scoped to
the BLE session. Services and their results survive disconnects. Wi-Fi/TCP remain
available independently; commands from either transport obey the same owner rules.

Fresh builds load `sdkconfig.defaults`: NimBLE, one peripheral connection, native
USB console, and `partitions.csv` with a `0x1f0000` application slot. The prior
1 MiB slot is too small for Wi-Fi plus BLE. NVS/PHY offsets are preserved and the
layout fits the first 2 MiB of flash. PSRAM is not required.

An existing ignored `sdkconfig` overrides defaults. When upgrading, select NimBLE,
enable `RAVEN_BLE_ENABLED`, select the custom partition table `partitions.csv`,
and choose USB Serial/JTAG for console output if using the native USB connector.
Run `idf.py reconfigure build` after configuration changes. The `RAVEN_RND_*`
symbols are retained for compatibility with existing configurations.

Boot self-tests default to off. Enable `RAVEN_RND_SELF_TEST` for the MCU suite;
advertising then begins only after the test sequence stops issuing demo commands.
The [runtime guide](rnd-runtime.md) lists build, flash and verification commands.

## Verification scope

Portable tests cover fragmented console input, validation/resynchronization,
independent controls, queue overload with HALT, and reconnect parsing/IDs. The
same cases run on the ESP alongside the prior runtime and real lwIP loopback tests.
`BLE_READY` confirms the host/controller accepted advertising configuration;
an actual phone connection and notification exchange are a separate radio check.
