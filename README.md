# Rucio ROOT Plugin

ROOT `TFile` plugin for opening Rucio DIDs directly:

```cpp
auto f = TFile::Open("rucio:///scope:name");
```

Use the native Rucio DID form `scope:name`, with three slashes after `rucio:`:

```text
rucio:///scope:name
```

The third slash is required.[^slashes]

The plugin resolves the DID through the Rucio REST API, receives PFNs, and then
opens the selected PFN through ROOT's existing transports, typically XRootD via
`root://...`.

The plugin is read-only. `CREATE`, `RECREATE`, `NEW`, and `UPDATE` modes are
not supported.

## Configuration

The plugin reads the standard Rucio client configuration and X509 proxy setup. In
a typical setup, `RUCIO_CONFIG` is the only Rucio-specific variable you need to
set, as long as its `[client]` section contains `rucio_host` and `account`.

| Variable | Required | Description |
| --- | --- | --- |
| `RUCIO_CONFIG` | Usually | Path to the Rucio client config. The plugin reads `rucio_host` and `account` from its `[client]` section. |
| `RUCIO_HOST` | Optional | Override for the Rucio REST base URL. Use this instead of, or in addition to, `RUCIO_CONFIG`. |
| `RUCIO_ACCOUNT` | Optional | Override for the account used when obtaining an X509 auth token. Use this instead of, or in addition to, `RUCIO_CONFIG`. |
| `X509_USER_PROXY` | Optional | Proxy path. If unset, `/tmp/x509up_u<uid>` is used. |
| `X509_CERT_DIR` | Optional | CA certificate directory. |
| `RUCIO_CA_PATH` | Optional | CA bundle file or CA directory override. |

With X509 authentication, the plugin obtains the REST token internally via
`/auth/x509`. You need a valid X509 proxy, but you do not need to export
`RUCIO_AUTH_TOKEN`.

## URL Options

URL query options refine replica selection:

```text
rucio:///scope:name?scheme=root&scheme=davs&rse=MY_RSE_EXPRESSION&domain=wan&sort=random&limit=3
```

Supported query keys:

- `scheme` or `schemes`: comma-separated or repeated list of schemes.
- `rse` or `rse_expression`: RSE expression.
- `domain`: `wan`, `lan`, or Rucio-supported value.
- `sort`: Rucio sort/selection hint such as `geoip` or `random`.
- `limit`: maximum PFNs requested from Rucio.
- `ignore_availability`: boolean.

## Build

```sh
cmake --workflow --preset release
```

The release preset builds the plugin and installs it under `install-release`.
To choose another installation prefix, configure the release preset with an
explicit install prefix and then build it:

```sh
cmake --preset release -DCMAKE_INSTALL_PREFIX=/path/to/prefix
cmake --build --preset release
```

Then make sure ROOT can find both the plugin macro and the library. For example,
add these entries to `.rootrc`:

```shell
Unix.*.Root.PluginPath: /path/to/prefix/etc/plugins:$(ROOTSYS)/etc/root/plugins
Unix.*.Root.DynamicPath: /path/to/prefix/lib:$(ROOTSYS)/lib/root
```

For development builds and tests, use the `dev` preset:

```sh
cmake --workflow --preset dev
ctest --preset dev
```

[^slashes]: `rucio:///scope:name` makes `scope:name` a URL path. Avoid `rucio://scope:name`, where URL parsing can treat `scope:name` as authority syntax instead of a DID.
