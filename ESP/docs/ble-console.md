# MC Service BLE Console

MC advertises `Raven-BLE`. It uses BLE GATT compatible with the HM-10 profile:
service `FFE0`, characteristic `FFE1`: Read, Write, Write Without Response, Notify.
This is the existing compatible [Bluetooth Serial Connect](https://bluetooth.henhen1227.com/wire-protocol.html) profile.
Subscribe to notifications before sending commands. The controller's BLE 5 version does not define the command format.

Send an LF-terminated line in Console. Both `help\n` and the app wrapper `C:help\n` are accepted; CR is ignored.
Responses use `D0:<text>\n`, making them visible in the terminal and on Text Display D0.

| Command | Response |
|---|---|
| `help` | Available commands |
| `status` | MC Device/Boot ID, readiness, number of Open sessions |
| `peers` | Registry of Ready/NotReady/Disconnected peers and generations |
| `peer <hex-id>` | Snapshot of identity, boot, role, version, route, reason, and last-contact time |
| `ping <hex-id>` | Admission of a manual probe, followed by its independent semantic outcome and RTT |
| `stats` | Internal DMA heap/free/min/largest, minimum free stack, stale/drops |
| `logs [count]` | Recent records with metadata and full local text; default 10 |
| `watch off` | Disable live events while retaining command responses |
| `watch on` | Watch new events from the current moment onward |

For example, `ping D0EF765CC8BC`. Obtain the ID from `peers`, not from the documentation example.
Arm angles, pair/halt demo commands, joystick mappings, and periodic telemetry panels have been removed.

Watch is enabled on connection. Healthy keepalive is silent. The terminal receives connection and
readiness transitions, errors, manual ping outcomes, and SDK warnings. Events are limited to
4 lines/s and wait until responses/transport are available. Multiline responses emit one line per
tick rather than filling the queue with a full dump. One long request is allowed per console
generation; the next receives busy.

The journal has a fixed capacity: 64 MC records / 32 node records. Identical adjacent events within
five seconds coalesce with a repeat count. `logs` shows the counter; loss of older records appears
as a gap. Live lines are compact; full local text is available through `logs`. Control characters
are replaced with spaces so that an error message cannot become a separate widget command.

An overlong/invalid input line is discarded in full through LF. Reconnect clears partial input
and prevents an old response from reaching a new generation. Overflow of a mandatory BLE response
or loss of an RX fragment closes only the BLE connection. TCP peer exchange continues.
The BLE console does not own PeerService deadlines and does not forward raw sockets.
