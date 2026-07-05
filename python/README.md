# Synsigra Python SDK

This package loads Synsigra challenge packages and verifies user algorithm
outputs locally.

Install from this directory:

```bash
python -m pip install .
```

Run one-command local verification:

```bash
synsigra-verify challenge.synsigra detections/ verification_out/
```

The customer-facing verifier uses only the downloaded challenge package and
the user's detection outputs. It does not require the signal generator source
tree.
