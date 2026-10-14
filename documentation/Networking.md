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
