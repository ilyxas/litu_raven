# Device ICD v1 and TCP Profile

The ICD describes devices and messages; it does not serialize `runtime::Message`, C++ layout, or
local endpoint IDs. `device_protocol.cpp` performs explicit big-endian encoding/decoding without
wire pointer casts. Internal messages contain a copied Body and local route. No borrowed RX buffer enters Inbox.

## Wire

TCP prefixes each packet with `uint16_be length`, excluding the two length bytes themselves. The
receiver supports arbitrary fragmentation and multiple frames in one recv. Length 0 or >64, invalid
codec data, version/type/flags close only the affected session. An incomplete frame has an absolute
1000 ms deadline from its first byte; another byte does not extend it. After an error, the parser
remains poisoned until reset.

| ICD header | Bytes | Meaning |
|---|---:|---|
| version | 1 | 1 |
| flags | 1 | bit 0: requires admission receipt; all others 0 |
| type | 2 | Message type |
| exchange | 4 | Nonzero correlation ID; Diagnostic permits 0 |

| Type | ID | Body bytes | ACK policy of current outbound contracts |
|---|---:|---:|---|
| HelloRequest | 1 | 48 | requires_ack |
| HelloResponse | 2 | 48 | no ACK |
| KeepAliveRequest | 3 | 20 | no ACK; a semantic response is expected |
| KeepAliveResponse | 4 | 20 | no ACK |
| Receipt | 5 | 4 | no ACK |
| Diagnostic | 6 | 16 + text_size, up to 48 | best effort, no ACK |

RX also accepts requires_ack on an incoming KeepAliveRequest. The internal TX contract sets the flag
by type rather than carrying arbitrary wire flags through the core. Responses and Receipt must not
set requires_ack. The largest current packet is 56 bytes, or 58 with TCP length. The codec limit of
64 leaves a small margin; contract changes require revisiting sizes and `static_assert`, not silent truncation.

The Hello body contains, in order:

```text
device_id:u64, boot_id:u64, capabilities:u16,
protocol:u8, role:u8, readiness:u8, reserved_flags:u8, reason:u16,
name:16 bytes, uptime_ms:u64
```

Device/Boot IDs are nonzero. Role: 1 MC, 2 Node. Readiness: 0 NotReady, 1 Ready, 2 Rejected.
Name is a fixed field; the wire format does not require a terminating NUL. Capabilities bit 0 denotes
the current peer exchange. Reserved flags must be zero. For Hello, protocol version is also available
inside the descriptor.

KeepAlive body: `origin_ms:u64, uptime_ms:u64, reason:u16, readiness:u8, reserved:u8`.
The response echoes the request's origin and includes its own uptime. These clocks are monotonic
and local; uptimes from two boards must not be compared as synchronized time. TimeSync is not implemented.

Receipt: `request_type:u16, admission:u8, reserved:u8` with the same exchange ID.
Admission: Accepted=0, Full=1, Invalid=2, Unknown=3. The receipt describes the recipient's local
dispatch, not peer readiness, operation outcome, or actual transmission of a response.

Diagnostic: `source_ms:u64, sequence:u32, code:u16, severity:u8, text_size:u8, text:0..32 bytes`.
Identity comes from the established session binding. The event retains its source timestamp and
local receive timestamp. This is a bounded notification; the ICD does not export the full remote journal.

## Timing and outcomes

```mermaid
sequenceDiagram
    participant Service as PeerService
    participant TX as TX Owner
    participant RX as Remote RX
    participant Peer as Remote PeerService
    Service->>TX: local admission + SendToken
    Note over Service,TX: local send budget 250 ms
    TX->>RX: entire frame accepted by local TCP stack (Written)
    Note over TX: write budget ≤100 ms, capped by remaining local budget
    RX-->>Service: Receipt: admission ACK/NACK
    RX->>Peer: message in Inbox
    Peer-->>Service: semantic Response
    Note over Service: deadline = first Written + 3000 ms
```

Receipt and Response pass through TX/Dispatcher on both sides; the diagram abbreviates the return
path. ACK and Response arrival order is not guaranteed. A valid Response can finish the exchange
even before the local writer publishes Written. Correlation uses route, generation, exchange, and type.

- Local TX admission does not mean Written. The 250 ms budget starts at reserve, immediately before
  the admission attempt. Dispatch rejection immediately ends that attempt with local rejection.
- Written means the TCP stack accepted the **entire** frame. It does not confirm remote recv or execution.
- The owner must process the response before the semantic deadline; queue time is part of that budget.
- Hello is retried 1 and 2 seconds after the first Written with the same exchange, at most three sends.
  ACK and subsequent Written records do not move the overall deadline. Each retry has its own local budget.
- KeepAlive: the next request is sent 1 second after a successful response; the response deadline is
  3 seconds. Successful cycles generate no events. A passive node closes a session without Hello after 5 seconds.
- RemoteNack, LocalRejected, SendDeadline, TransportLost, and ResponseTimeout are distinct. Timeout
  never synthesizes a "received NACK".

Hello/KeepAlive are domain-idempotent: a repeated Hello preserves the binding and returns readiness;
a repeated KeepAlive returns fresh status. A completed exchange is not reopened. An exactly-once
operation journal and universal deduplication cache were not added; a different contract may require them.

## Overload and recovery

All current outbound messages use Ordered. Diagnostic additionally permits at most one pending
message per session and is source-limited to four events per second. Rejected optional outbound
admission increments a counter; losing a mandatory ACK/Response closes the session. If a command
was accepted but its ACK no longer fits in TX, the connection closes explicitly: receipt delivery
cannot be promised without an available outbound channel.

Sending without an Open route is rejected; no disconnected queue accumulates data. A new connection
starts a new handshake. The node's TcpManager uses the DHCP gateway and backoff of
250/500/1000/2000/5000 ms + jitter 0..100 ms; losing a live session also introduces a delay.
A socket must close after a partial write; the remainder of a frame never continues in another session.

The TCP memory profile uses send/window 2880, MSS 1440, a disabled out-of-order queue, and L2→L3 copy.
Copying releases the shared Wi-Fi RX buffer before waiting for the socket consumer.
`communication_profile.cmake` defines lwIP queue/write thresholds; the sole writer checks `select()`
before every short frame. Rationale and hardware measurements are in [validation.md](validation.md).
