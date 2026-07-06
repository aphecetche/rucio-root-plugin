# Rucio ROOT Plugin

Minimal ROOT `TFile` plugin for opening Rucio DIDs transparently:

```cpp
auto f = TFile::Open("rucio:///scope:name");
```

ROOT-safe DID URL forms are `rucio:///scope:name` and `rucio://scope/name`.
The visually natural `rucio://scope:name` form is accepted by the standalone
parser, but ROOT's `TUrl` treats `scope:name` as URL authority syntax before the
plugin sees it.

The plugin resolves the DID through the Rucio REST API, receives PFNs, and delegates the actual I/O to ROOT's existing transports, typically XRootD through `root://...`.

This MVP is read-only. `CREATE`, `RECREATE`, `NEW`, and `UPDATE` intentionally fail until upload and DID registration semantics are designed.

## Configuration

The resolver reads the standard Rucio client configuration and X509 proxy setup.
For normal KM3NeT use, `RUCIO_CONFIG` plus the usual X509 environment should be
enough:

- `RUCIO_CONFIG`: Rucio client config. The plugin reads `rucio_host` and `account` from its `[client]` section.
- `RUCIO_HOST`: optional override for the Rucio REST base URL when no config value should be used.
- `RUCIO_ACCOUNT`: optional override for the account used when obtaining an X509 auth token.
- `X509_USER_PROXY`: optional proxy path. If unset, `/tmp/x509up_u<uid>` is used.
- `X509_CERT_DIR`: optional CA certificate directory.
- `RUCIO_CA_PATH`: optional CA bundle file or CA directory override.

With X509 authentication, the plugin obtains the REST token internally via
`/auth/x509`; users do not need to export `RUCIO_AUTH_TOKEN`.

URL query options can refine replica selection:

```text
rucio:///scope:name?scheme=root&scheme=davs&rse=MY_RSE_EXPRESSION&domain=wan&sort=random&limit=3
```

Supported MVP query keys:

- `scheme` or `schemes`: comma-separated or repeated list of schemes.
- `rse` or `rse_expression`: RSE expression.
- `domain`: `wan`, `lan`, or Rucio-supported value.
- `sort`: Rucio sort/selection hint such as `geoip` or `random`.
- `limit`: maximum PFNs requested from Rucio.
- `ignore_availability`: boolean.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Install the library and plugin macro somewhere visible to ROOT. The installed plugin macro registers `rucio://` URLs as a `TFile` protocol handled by `libRucioROOT`.

```sh
cmake --install build --prefix /path/to/prefix
```

Then make sure ROOT can find the library and plugin macro. For example, add a ROOT plugin path entry in `.rootrc`:

```sh
printf 'Unix.*.Root.PluginPath: /path/to/prefix/etc/plugins:%s/plugins\n' "$(/opt/homebrew/bin/root-config --etcdir)" >> ~/.rootrc
```

and make the library visible to the dynamic loader:

```sh
export DYLD_LIBRARY_PATH=/path/to/prefix/lib:$DYLD_LIBRARY_PATH
```

After that, no explicit `gPluginMgr->AddHandler(...)` call is needed.
