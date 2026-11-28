# RavenChick

`chick_mc` and `chick_body` are reserved for independent ESP-IDF device projects.
Each device owns its own:

- `Bootstrap` — composition, wiring, and resource lifetimes.
- `Activities` — product scenarios and orchestration.
- `Services` — product-specific subsystem behavior.
- `Configuration` — device build and resource configuration.

This preparation contains directory placeholders only. ESP-IDF project definitions,
entry points, and product functionality will be added with their implementation;
these directories are not firmware build targets yet. Reusable components are in
[ESP/components](../../components); the reference bootstrap under
`ESP/examples/device_communication` is not a product dependency.
