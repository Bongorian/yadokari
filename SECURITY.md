# Security policy

## Reporting a problem

If you find a security problem in YADOKARI — for example in the
update-signature verification, the Wi-Fi file transfer, or the
file parsers — please report it privately first:

- Use GitHub's **private vulnerability reporting** on this
  repository (Security tab → "Report a vulnerability"), or
- contact the maintainer through the address on the GitHub profile.

Please don't open a public issue for an exploitable problem until
we've had a chance to look at it. This is a hobby project with one
maintainer, so replies may take a few days — but reports are taken
seriously and credited in the fix notes when the reporter wishes.

## Scope notes (honest edition)

- Firmware updates are **signed (Ed25519)** and verified on the
  device before installation. The signing model uses a single
  baked-in key with no rotation; see the README's "Update signing"
  section for what that means for self-builders.
- The Wi-Fi file transfer is currently **provisional and
  unencrypted** — use it on networks you trust. It is listed in
  [known limitations](docs/known-limitations.md), not hidden.
- The MASCHINE MK3 itself is never modified; YADOKARI runs
  entirely on the Raspberry Pi.
