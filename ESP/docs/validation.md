# Verification and Resources

Current stage date: November 26, 2026. Windows host, ESP-IDF 6.1.1. The physical board count is not
an architectural limit. Portable tests, real TCP tests, and physical-device tests are distinguished below.

## Reproduction

Host suite:

```sh
cmake -S ESP/tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure --timeout 30
# Linux: separate directories for the two tools:
cmake -S ESP/tests/host -B build/asan -DRAVEN_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S ESP/tests/host -B build/tsan -DRAVEN_SANITIZER=thread -DCMAKE_BUILD_TYPE=Debug
```

The `communication_test` CTest executable contains 17 scenarios: golden codec/invalid input; Critical admission,
coalescing, and ticks under flood; fragmented/coalesced framing and absolute timeout;
handshake/keepalive/probe; admission/Written/semantic deadline; response before Written; stale
RX/TX after reuse; concurrent close versus commit; lost keepalive and ACK with full TX; bounded
journal; parser/reconnect; send tickets, I/O leases, and 200 reuses; six peers/duplicate identity;
at most one optional diagnostic; SDK Log V1/V2 fragments; transport-independent PacketWriter;
full logs text.

The `service_contract_test` executable adds three scenarios: independent State snapshots
across close/reuse, concurrent snapshot consistency, and DiagnosticsService using State
and Dispatcher without a PeerService implementation dependency. The latter covers an
unbound route, full admission, and a separately executed probe/result exchange.

`RAVEN_TCP_BOARD_PROBE=y` in a **node test build** replaces the ordinary composition with a test client.
`RAVEN_VALIDATION_MONITOR=y` adds UART snapshots every 10 seconds and an additional task with a
3072-byte stack; both options default to off. This is neither a service command nor part of the production protocol.

For the hardware TCP suite, ordinary nodes are temporarily stopped, MC runs with capacity=6, and one
test node joins the AP and creates six sessions with explicitly test-only IDs. Automated checks cover:
20 seconds of handshake/keepalive for all peers; independent close/reconnect; duplicate identity
without takeover; unsupported version, invalid length, partial-frame deadline; a non-reading peer
under load; and subsequent recovery of all six. Healthy-session checks require fresh keepalive
(less than 3 seconds old), not merely an open fd and a previous handshake. The suite is repeated
separately with a connected BLE console. After testing, restore `RAVEN_TCP_BOARD_PROBE=n` and ordinary node firmware.

UART capture does not send application commands:

```powershell
python ESP/tests/capture_serial.py COM4 COM5 COM6 COM7 --seconds 1835 --output .cache/capture
# --reset COM5: ordinary RTS/EN reset
# --rom-reset COM4: ROM/esptool reset for native USB
python ESP/tests/check_capture.py .cache/capture --mc COM4 --nodes COM5 COM6 COM7 --seconds 1800
```

`check_capture.py` excludes the first 20 seconds of recovery, then requires continuous snapshots,
three Ready bindings on MC and one on each node, increasing last_seen, unchanged Boot ID/generation,
resource thresholds, and no reset/watchdog/timeout. Automated board stop/return uses
`--actions ESP/tests/four_boards.example.json`: COM numbers in this **bench scenario** must be adapted
to the actual connections. It does not define the application's board list. The stable interval
after the final recovery is checked separately.

## MC memory budget, capacity=6

This table records the profile used for the four-board hardware run. The later role/State
correction adds 16 bytes to Runtime; its compiler measurements are recorded below.

`sizeof` values were measured by the Xtensa compiler and confirmed by the ESP32-S3 startup log.
References/queues are not dynamically allocated for each message. Runtime owns the slot tables.

| Object | Size, bytes | Count / contribution |
|---|---:|---:|
| Message | 88 | payload 64; previously 48 with payload 24 |
| Inbox | 1 520 | 9 = 13 680; previously 880 each |
| Dispatcher | 64 | 16 local endpoints; previously 32 |
| Session | 136 | 6 = 816; includes two SendRecord entries of 48 each |
| PeerSlot | 240 | 6 = 1 440 |
| PeerRecord | 72 | 12 = 864 |
| Journal Record | 128 | 64 = 8 192 |
| TcpEndpoint | 1 864 | 6 = 11 184; **includes** TX Inbox, do not count it twice |
| BLE transport | 4 200 | bounded RX×4/TX×8, control blocks, and active frame |
| Entire Runtime | 32 240 | all of the above + services/owners/parsers/references; excludes task stacks |

Payload 64 is determined by `Inbound{route, receive_time, Body}` and `Outbound{route, SendToken, Body}`:
both are exactly 64 bytes. Body 48 is determined by Hello, not arbitrary future bulk traffic.
Sizes are checked with `static_assert`; contract changes require a new calculation.

| Application tasks | Stack bytes |
|---|---:|
| 6 TCP RX + 6 TCP TX | 36 864 |
| PeerService + DiagnosticsService | 8 192 |
| TCP manager + BLE RX + BLE TX | 9 216 |
| Total: 17 tasks | 54 272 |
| TCB, mutexes, RTOS metadata: reserve | 4 096 |

SDK tasks/radio pools are additional to these application stacks: Wi-Fi stack 6656, NimBLE host 4096,
lwIP task 3072 bytes, event/controller/idle/timer tasks, and SDK globals are accounted for in the link
map and heap measurements after startup. NimBLE MSYS1/MSYS2/ACL pools are configured for 8 blocks each;
Wi-Fi static RX/dynamic RX/dynamic TX each have 6. PSRAM is not used.

Dynamic communication-profile allocations, with conservative reserves beyond empty sessions:

| Resource | Reserve, bytes | Basis |
|---|---:|---|
| TCP TX | 19 200 | 6×2 pbuf up to 1600 bytes including MSS/header/metadata |
| TCP RX with L2→L3 copy | 20 736 | 6×2880 window + 36×96 metadata/header |
| Shared Wi-Fi dynamic RX | 12 288 | 6×2048 for alignment/full frame |
| Shared Wi-Fi dynamic TX | 12 288 | 6×2048; Wi-Fi copies TX |
| TCP/netconn/mailboxes/allocator | 6 144 | six bindings, SDK queues, and metadata |
| Connect/close transient reserve | 4 096 | concurrent lifecycle transitions |
| Connected BLE client | 6 144 | beyond startup radio pools; observed difference about 4.7 KiB |
| Total dynamic reserve | 80 896 | do not subtract Runtime/stacks twice from already measured free heap |

In the final production firmware, startup free internal DMA heap is 110 664 bytes after radio and
application owners start. Returning from `app_main` frees another 3584 bytes of its stack
(`CONFIG_ESP_MAIN_TASK_STACK_SIZE`); the returned TCB is not counted as reserve. Production estimate:
110 664 + 3584 − 80 896 = **33 352 bytes**. This is a conservative reserve for this ICD/window/profile,
not a guarantee under arbitrary network load. Allocation overhead and fragmentation are checked
on hardware using free/min heap, largest block, and stack high-water marks. The test monitor task
additionally consumes a 3072-byte stack and a TCB; this is included in the hardware measurements
below but not in the production profile.

Bench criteria: at least 32 KiB internal DMA heap, largest block ≥8 KiB, and free stack of every
application owner and the NimBLE host ≥1 KiB. Recheck these criteria when increasing capacity.

### Rationale for the lwIP settings

ESP-IDF 6.1.1 sets `LWIP_NETIF_TX_SINGLE_PBUF=1`; TCP_OVERSIZE cannot be disabled with it because the
SDK will not build. `tcp_pbuf_prealloc` reserves a full MSS even for a short frame. Therefore,
`TCP_SND_BUF=2880` alone is insufficient to budget many small sends.

In `communication_profile.cmake`: `TCP_SND_QUEUELEN=4`, `TCP_SNDQUEUELOWAT=2`, `TCP_SNDLOWAT=128`.
There is one writer, which runs select before each frame ≤66 bytes. lwIP `api_msg.c` removes
writability at two queued pbuf; the next write occurs after space becomes available. Thus, one
writer's data pbuf are limited to two. TCP/Wi-Fi copies are counted separately. SDK sources are unchanged.

The first hardware slow-reader run reproduced shared RX-pool exhaustion on the test node with
`L2_TO_L3_COPY=n`: its other sessions lost heartbeat. The same test passes after enabling copy;
MC closes only the slow stream with `send-deadline`. SDK `wlanif.c` confirms release of the L2 buffer
after copying, and retained L3 bytes are included in the TCP RX budget above.

## Recorded stage results

- Host: 17/17 scenarios pass on Windows (Zig 0.15.2 / Clang 20.1.2, C++17, warnings as errors).
- ESP-IDF: MC ESP32-S3 and node ESP32 built. The real Wi-Fi TCP suite completed with all six PASS,
  without MC reset/panic/watchdog. After the L2-copy fix, the other five peers survive the slow reader.
- MC with six TCP sessions and BLE advertising: minimum internal DMA heap **93 568**, largest
  sampled block **65 536**, minimum sampled owner/host stack **1 252** bytes. These include the validation task.
- The user confirmed readable BLE `help/status/stats/logs/watch off` responses and left the app connected.
- Repeating the same full TCP suite with active BLE completed with six PASS. MC: minimum heap **87 244**,
  minimum sampled largest block **63 488**, minimum sampled stack **1 252** bytes.
- Four physical boards initially completed the shared handshake: MC `E072A1D72014` (COM4), nodes
  `08A6F76BBA34` (COM5), `246F2836590C` (COM6), and ESP32-CAM `D0EF765CC8BC` (COM7 after moving USB).
  CAM reproducibly lost serial response, recovered by removing power; this is a separate bench limitation.
- Each node was stopped individually for 15 seconds and returned without disrupting other links;
  MC was stopped/returned while nodes remained running; COM5, COM7, and COM6 were restarted in sequence.
  Each returned with a new Boot ID; MC reused slots with a new generation. Stops held the application
  in ROM through esptool; the user additionally physically power-cycled CAM.

- [CI for 3290f57](https://github.com/ilyxas/litu_raven/actions/runs/45737377589): all six jobs passed —
  Linux host, ASan/UBSan, TSan; MC ESP32-S3, node ESP32, and node ESP32-S3 firmware.

The final continuous four-board capture lasted 1835 seconds; after excluding the first 20 seconds
of recovery, `check_capture.py --seconds 1800` passed for every board (181–182 snapshots).
All links remained Ready, last_seen increased, and Boot ID/slot/generation stayed unchanged;
there were no resets, watchdogs, timeouts, or new diagnostic drops during the interval. COM6's
counter already contained one startup diagnostic drop and did not increase. BLE was connected
from second 127 through the end of the capture.

| Board | Minimum internal DMA heap | Minimum sampled largest block | Minimum owner/host stack |
|---|---:|---:|---:|
| COM4 / MC | 91 860 | 63 488 | 1 312 |
| COM5 / node | 198 192 | 110 592 | 1 480 |
| COM6 / node | 198 068 | 110 592 | 1 472 |
| COM7 / CAM node | 197 156 | 110 592 | 1 544 |

After the soak, the full TCP suite was repeated with stronger keepalive-freshness checks and the
same BLE connection, without restarting MC: **6/6 PASS**. The slow reader was closed with
`send-deadline`; the other five sessions continued exchanging messages, then all six were restored.
On this warmed-up MC with reused sessions, minimum heap was **82 632**, largest sampled block
**59 392**, and minimum stack **1 256** bytes. No MC reset/panic/watchdog or new diagnostic drops
occurred. This run recorded the stage's lowest heap.

All four boards were left with ordinary firmware using `RAVEN_VALIDATION_MONITOR=n` and
`RAVEN_TCP_BOARD_PROBE=n`: MC image 1 054 240 bytes, node image 826 640 bytes. Esptool verified each
board's flash by hash; CAM was flashed through the ROM loader with `--no-stub`. Subsequent 60-second
observation after MC reset confirmed all three nodes Ready and BLE_READY, no test output, and no
new errors after link recovery. Serial ports were released.

Earlier "25/25 on Arm Demo" reports do not validate this implementation. No real iOS client or UART
transport is present in this stage; their operation is not claimed.

## Role and State correction verification

The correction after `382a2df` restores semantic Activity/Service bases and removes
DiagnosticsService's dependency on PeerService. See
[ADR-004](adr/0004-explicit-roles-and-peer-state.md) for scope and preserved contracts.

- Windows host: both CTest executables pass, covering the original 17 communication
  scenarios and three new service-contract scenarios; compilation uses warnings as errors.
- ESP-IDF 6.1.1: MC ESP32-S3 and node ESP32 build successfully. Local image sizes are
  1 055 088 and 827 504 bytes, respectively.
- The MC ELF reports Runtime at 32 256 bytes versus the earlier 32 240 (+16).
  Message remains 88 and Inbox 1 520 bytes. The peer-state mutex is moved, not duplicated;
  task counts, configured stacks, queue capacities, and wire payloads do not change.
- Applying only that static-memory delta to the earlier conservative reserve yields
  33 336 bytes (33 352 − 16). This is a derived budget adjustment, not a new heap measurement.
- The immutable architecture baseline and normative specification are unchanged.

No board was reflashed for this correction. Earlier hardware soak, BLE, heap, and
stack results above remain evidence for the preceding firmware, not a fresh hardware
run of these changes. Whole-PR architectural compliance is a separate review.
