# Monocypher

Unmodified files from LoupVaillant/Monocypher tag 4.0.3:
https://github.com/LoupVaillant/Monocypher/tree/4.0.3/src

`monocypher.c/h` and optional `monocypher-ed25519.c/h` implement standard
Ed25519 verification in the firmware updater. The included `LICENCE.md`
offers BSD-2-Clause or CC0; this project uses BSD-2-Clause. Preserve notices.
Signing uses the host's OpenSSL CLI; private keys are never firmware assets.

Pinned commit: ab2b16dd619ad5f6979a4fbe69cfa324a6fcc35f. Source file hashes are in SHA256SUMS.
4.0.3 was selected after checking upstream releases and the EdDSA/Ed25519
timing fix. Firmware uses verification only; signing remains host OpenSSL.
