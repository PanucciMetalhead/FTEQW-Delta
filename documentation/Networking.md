# Networking reliability changes

Connection setup retries after 1, 2, and 4 seconds, then uses the previous
5-second interval. When DNS returns multiple addresses, the first pass tries
alternate answers at 250 ms intervals. Failed initial UDP routes advance to the
next answer. These changes only affect connection setup.

DNS workers capture the requested port and a generation number. Results from
aborted or superseded connections are ignored and freed, including reconnects
to the same hostname. Completed resolution contexts are also freed.

UDP sends report actual failures rather than falling through to success.
Interrupted calls and temporary socket buffer exhaustion are handled as
transient errors. Oversize/reset diagnostics are throttled, and ICMP errors on a
shared UDP socket no longer trigger a disconnect/reconnect of an unrelated game
connection. Existing netchan connection timeouts still apply.

Expired and failed DTLS peers use the regular unlink/destruction path, repairing
list back-links and freeing the peer. GnuTLS sessions use `net_mtu` consistently
instead of different hardcoded client/server limits, check the negotiated usable
payload size, and propagate MTU/temporary transport errors. Blocked encrypted
datagrams are discarded from GnuTLS's queue because netchan owns retransmission;
a later packet must not flush stale data. Handshake maintenance observes the
GnuTLS retransmission timer. See the [GnuTLS record API](https://www.gnutls.org/manual/html_node/Data-transfer-and-termination.html)
and [DTLS API](https://www.gnutls.org/manual/html_node/Datagram-TLS-API.html).

Truncated netchan headers are rejected before updating sequence, acknowledgement,
or fragment state. Decrypted records that fill the receive buffer are rejected,
leaving room for the parser sentinel and preventing partial packet parsing.
Connection challenge tokens use OS randomness when available. The two legacy
PRNG draws are retained so the change does not advance unrelated game randomness
differently; the existing random-token fallback remains if OS entropy fails.

Movement, prediction, interpolation, physics, user-command generation, gameplay
packet pacing, state encoding, and protocol negotiation formats are unchanged.
Existing encryption preferences and certificate verification remain in effect.
These fixes do not add encryption support to legacy servers.

## Packet packing and startup checks

Instrumented Remaster client/server startup exposed signed shifts in reliable
header flags, connection cookies, solid-box fields, native byte swaps and
long/double message decoding. These paths now use unsigned bit operations;
connection-cookie clock bytes are copied without type aliasing. Existing
sequence/reliable flag layouts and solid-box bounds are preserved. The double
reader reconstructs all eight bytes without sign-extending its low word.

The additional data-free fixtures check exact decoded fields, truncated input,
all reliable/ack flag combinations, every encoded solid-box height and high-bit
cookie/swap patterns. Run `tools/parser_sanitizer_tests.py` as described in
[SafetyChecks.md](SafetyChecks.md). The owned-data
[Remaster checks](QuakeRemaster.md) also exercised local client/server joins.
These changes do not modify movement or the retransmission algorithm.

## Package signatures

Package mirrors and HTTPS connections no longer grant publisher trust. Native
package downloads normally require a verified individual signature or a
hash/size entry in a trusted catalog; unsigned data archives keep their existing
restricted rules. See [PackageTrust.md](PackageTrust.md) for the complete policy,
existing explicit overrides, and release/debug regression checks.

## Native transports and file transfers

The desktop engine retains UDP, TCP/TLS, WebSocket, ICE/WebRTC, and HTTP/HTTPS
support when their dependencies are enabled. Outgoing URL downloads use the
native transfer implementation. The dedicated server can serve permitted game
files over HTTP; this follows its existing `allow_download` and file-type
settings. QTV retains its status/admin pages and `.qtv` stream links.

`sv_port_tcp` selects the listening TCP interface and port. In the client and
dedicated-server builds, `net_enable_http` defaults to `0`; set it to `1` to
accept HTTP requests. It does not affect outgoing HTTP/HTTPS downloads.
`net_enable_websockets` defaults to `1`, but accepting connections still
requires a listening TCP port. The standalone master-server build uses different
defaults: HTTP enabled and WebSocket game connections disabled. ICE and WebRTC
signaling continue to use the native networking implementation.

## Validation

```sh
python3 tools/network_transport_tests.py
python3 tools/network_integration.py /path/to/Quake
```

The transport harness compiles production functions directly with test fixtures
and AddressSanitizer/UndefinedBehaviorSanitizer. It checks socket failures,
cancelled/stale DNS results, challenge entropy and PRNG advances, DTLS peer
cleanup, truncated headers, malformed fragments, duplicates, loss and recovery.
The GnuTLS fixture uses real encrypted records to check payload MTUs, blocked-send
fidelity, modified-ciphertext rejection, and receive bounds. It requires Clang,
`pkg-config`, and GnuTLS development files.

The integration test launches a local dedicated server and SDL GPU client with
owned Quake data linked into an isolated directory. Its UDP relay drops the
first request burst, then introduces 5% packet loss, duplicates and reordering.
The updated plaintext client also receives ten malformed headers. Both plain
and encrypted connections must join and deliver reliable chat. A separate DTLS
case supplies an incorrect certificate fingerprint and must fail without
joining or downgrading to plaintext. Test certificates and keys are generated
only in the isolated output directory.

macOS validation passed the sanitizer/real-GnuTLS tests. In the local comparison,
losing the first request delayed the old client's retry by 4.99 seconds and the
updated client's by 1.01 seconds. Both clients joined and delivered reliable chat
under the relay's losses; the updated client also survived the malformed headers.
The encrypted connection passed with every captured datagram at or below its
1200-byte configured MTU, and the incorrect fingerprint was rejected.

Additional macOS checks verified byte-exact native HTTP file serving and
outgoing downloads, plus a native WebSocket game connection that joined and
delivered reliable chat. These checks used isolated local servers and game data.

Client, dedicated server, and master-server targets built successfully. Native
Windows/Linux runtime testing remains pending. These local measurements describe
connection retry recovery, not Internet round-trip latency or a full security
audit. `--baseline /path/to/old/fteqw` enables the optional retry comparison;
`--client`, `--server`, and `--out` override test locations. All generated output
stays in the ignored build directory by default.

## Dedicated-server performance

Build the release dedicated server and QuakeC compiler, then run the synthetic
collision workload:

```sh
cmake --build build/all --config Release --target fteqw-sv fteqcc
python3 tools/server_benchmark.py --server build/all/fteqw-sv --qcc build/all/fteqcc
```

On multi-configuration builds, pass the paths to the release executables.
Each case runs in a fresh process at a requested 120 Hz with 64 static
blockers on an original synthetic BSP. It compares 0, 256 and 1,024 collision
traces per physics tick after a two-second warmup. It needs no game assets,
client, GPU or public listener. Use `--traces` or `--frames` to change the load or
sample count. Logs and JSON results are kept in a fresh run directory.

The server console command `sv_benchmark [frames]` measures work duration on
frames that advance physics. A frame can advance more than one physics tick on
other workloads. The output includes mean, p50, p95, p99, max, and the elapsed
wall time. The work duration excludes sleep between frames. The synthetic
fixture measures QuakeC and collision work without connected clients, so it
does not measure snapshot throughput or network latency. Run it separately
from renderer benchmarks to avoid CPU contention.

On 2026-11-21, a macOS arm64 Release run recorded these results:

| Traces per tick | Frames | p99 work | Worst work | Sample wall time |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 600 | 0.104 ms | 0.241 ms | 6.356 s |
| 256 | 600 | 0.403 ms | 0.763 ms | 7.054 s |
| 1,024 | 600 | 1.341 ms | 1.970 ms | 7.330 s |

The 120 Hz figure is a setting for the fixture. The wall times include the
server loop's sleep and scheduling delays.

### QuakeWorld client scaling

```sh
python3 tools/network_scaling.py /path/to/Quake --clients 1 4 8 16 --client-rate 60
```

The benchmark uses headless clients and a dedicated server on one host.
Each run uses the `start` map with 32 server slots and a requested 13 ms
physics interval. Clients join through separate loopback UDP sockets and
hold movement and fire controls. The server starts sampling after all
clients have entered. Use `--client` and `--server` to select builds.
At 30 or 60 packets per second, the benchmark launches clients with
`-headless-active`, which lets the normal client frame limiter set the
pace. Use `--client-rate 10` to run with the default headless throttle.
Each client must sustain at least 75% of the requested send rate during
the sample for the run to pass.

The output directory contains server timing percentiles, per-client UDP
downlink payload bytes, datagram counts in both directions, and logs. Downlink payload includes game
traffic such as snapshots and reliable messages, and excludes IP and UDP
headers. Client processing and the UDP relay share CPU resources with
the server in this test.

For a sustained session, choose up to 8,192 physics-bearing frames and
enable periodic samples with `--sample-interval`:

```sh
python3 tools/network_scaling.py /path/to/Quake --map dm6 --clients 32 \
  --frames 8192 --sample-interval 10
```

The resource samples record server resident memory in KiB using `ps`
alongside cumulative UDP payload and packet counters. Subtract successive
counter values to compare traffic across the session.

To check client replacement and map rotation on a full server:

```sh
python3 tools/network_scaling.py /path/to/Quake --map dm6 --rotate-map dm2 \
  --clients 32 --reconnect-clients 8 --frames 600
```

The harness samples the initial map, removes the selected clients through the
server console, and fills their slots with fresh client processes. It then
issues `changelevel` with the full roster online and waits for every player
to enter the new map before sampling again.

## Live QuakeWorld gameplay under loss

```sh
python3 tools/network_gameplay.py /path/to/Quake --client build/fteqw --server build/all/fteqw-sv
```

The benchmark uses a dedicated server, one moving client, and a loopback-only
UDP relay. It measures a clean connection, one with 15–105 ms of one-way
delay and three downlink outages, and the same impaired link with a 1,200
payload bytes/second downlink and a bounded queue. Each run checks that
reliable chat reaches the server during the measurement and saves both
process logs and `results.json` in a fresh directory.

`net_benchmark start` and `net_benchmark stop` report wall-clock gaps between
accepted QuakeWorld world updates. The client keeps the latest 4,096
intervals for percentiles and tracks the maximum for the entire run.
Updates rejected by the entity parser are counted separately from missing
datagrams. Prediction counters cover the primary player and distinguish
frames that fall back because command history is missing or exhausted
from frames whose history search found no matching base. These counters
measure prediction inputs and are different from measuring visual corrections.
The server also reports work time on frames that advanced physics.

Use `--headless` to exercise snapshots and chat without a display. Headless
mode can report different processing gaps and omits rendered prediction
samples. Use `--classic-qw` to disable negotiated protocol extensions.
Use `--profiles limited --limited-client-rate 1000` to try a lower
client-requested server send rate on the same constrained link. The
fixture enables QuakeWorld movement prediction explicitly because the
stock game data otherwise selects the compatibility mode for NetQuake mods.

On 2026-11-21, a macOS arm64 Release run with a Metal client measured:

| Link over 12 seconds | Accepted snapshots | p99 snapshot gap | Worst gap | History fallbacks | p99 server work |
| --- | ---: | ---: | ---: | ---: | ---: |
| Clean loopback | 722 | 24.6 ms | 34.2 ms | 0 | 0.299 ms |
| Jitter and outages | 247 | 116.5 ms | 998.9 ms | 1 | 0.495 ms |
| Jitter, outages and 1,200 B/s downlink | 245 | 117.2 ms | 1,099.0 ms | 1 | 0.530 ms |

Reliable chat arrived in all three cases. The 900 ms outage drove the largest
gaps in this fixture. Server work stayed below 1 ms at p99 across the three
cases. The measurements describe a single local client and an emulated link.
A separate constrained run requested 1,000 B/s after joining. It accepted
242 snapshots, with a 132.4 ms p99 gap and one history fallback. That single
comparison provides no evidence for lowering the default send rate.

### Snapshot recovery on an asymmetric link

```sh
python3 tools/network_gameplay.py /path/to/Quake --profiles recovery --client build/fteqw
```

The recovery profile asks the client for 60 sends per second, adds 220 ms
uplink and 30 ms downlink delay, and drops server-to-client packets for 850 ms.
It exercises the interval where the outgoing history has aged out a delta
request but a snapshot already in flight can still decode against the most
recent received snapshot. Use `--client` to compare builds on the same link.

The client now keeps the last decoded replacement snapshot after it stops
advertising that snapshot as a delta base. The incoming parser still
checks whether the base fits its receive history. If movement command
history has run out while waiting, the camera holds its last predicted
position until an update arrives.

In a macOS Metal comparison at the same requested packet rate, the previous
client's longest accepted-snapshot gap was 1,074.5 ms and it rejected 12
updates. With the fix the gap was 864.4 ms and it rejected none. Both clients
delivered reliable chat. The comparison measures snapshot recovery; the
prediction counters also depend on whether a usable world snapshot is present
when a frame begins.
