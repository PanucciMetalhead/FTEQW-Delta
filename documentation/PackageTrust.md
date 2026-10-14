# Package download trust

The package manager (`pkg` and `menu_download`) no longer accepts a package
because its mirrors have a particular URL prefix. Release and debug builds use
the same signature policy. HTTPS protects the download connection; it does not
establish who published a package.

## How a new download becomes trusted

A package can have its own signature over its SHA-512 download hash. If an
archive prefix is supplied, the signature instead covers SHA-512 of the prefix,
a NUL separator, and the binary download hash. The public certificate must be
in the engine's authority list (`Auth_GetKnownCertificate` in
`engine/common/net_wins.c`), and a crypto provider must verify the signature.
Legacy signatures without an authority prefix still use the `Spike` authority.
No publisher keys or certificate authorities were added by this change.

Alternatively, a verified signature of a complete package catalog can authorize
its entries. Each entry needs a nonzero download size and a complete SHA-512 or
legacy SHA-1 hash. SHA-512 is preferred for new catalogs. A missing provider or
unknown signer cannot establish catalog trust. A bad individual signature or
malformed metadata is rejected even when the parent catalog is trusted.

Hash strings must contain exactly 128 hexadecimal characters for SHA-512 or
40 for SHA-1. Signatures must be complete Base64 values that fit the verification
buffer. Standard and URI-safe alphabets, with optional padding, are supported;
invalid characters, invalid padding, and oversized values are rejected instead
of accepting a decoded prefix. Revalidation clears previous signature results.
During download, the existing write filter checks the received hash and byte
count before installation.

Without that trust, new executable/plugin packages and unrestricted ZIP
extraction remain blocked. Unsigned data-only archives retain the existing
restricted installation rules: a game directory, permitted archive extension,
and the required archive checksum, cache-file declaration, or manifest context.
This preserves ordinary PAK/PK3 mod downloads; it is not a sandbox for mod code.

## Existing explicit trust

This change removes automatic mirror/TLS trust. It does not revoke packages
already present on disk or trust saved in the local `installed.lst` file.
The engine still recognizes installer-trusted manifests. The explicit
`-updatesrc URL -unsafe` launch override also remains: it tells the package
manager to trust that source rather than require its catalog signature.
Normal launches do not enable this override. The former `-notlstrust` switch
is removed because there is no longer a URL-based exception to disable.
Upstream binary self-updates remain disabled in FTEQW Delta.

## Regression checks

GCC/Clang and Python 3 can run the standalone ASan/UBSan checks without game
data, a display, or a remote server:

```sh
python3 tools/package_signature_tests.py --out build/package-tests
```

The tests extract the actual authenticity, decoding, and native/data
installation-gate functions from the current source and compile them both with
and without `_DEBUG`. Host structures, authority lookup, and provider outcomes
are supplied by the fixture. Cases cover former trusted-site mirrors, mixed
mirrors, missing/unknown/bad signatures, provider fallback, stale flags,
malformed hashes/encodings, catalog trust, and unsigned data archives.

With GnuTLS development files and `pkg-config`, also run:

```sh
python3 tools/package_signature_tests.py --gnutls --out build/package-crypto-tests
```

This adds the engine's actual GnuTLS verifier. Ephemeral RSA credentials test
genuine SHA-512 signatures and rejection of changed hashes, signatures, and
archive prefixes in both configurations. No private keys are saved or shipped.
All four variants passed on the owner's Mac. The policy fixtures run through
sanitizer CTest on Linux/macOS; CI additionally runs real GnuTLS checks on Linux.
The tests do not simulate a complete HTTP catalog/install transaction or native
Windows certificate verification.
