# Firmware updates

The implementation entry points are the [Manager](../src/update/manager.h), the [runtime](../firmware/update_runtime.cpp), the [recovery](../firmware/recovery) and the [host tools](../tools/update). The protection boundary is the [threat model](update-threat-model.md), and the scope confirmed on real hardware is consolidated in the implementation status.

## Invariants

- An online update replaces only `KERNEL.IMG` in the inactive slot. The immutable recovery and DATA are never replaced.
- The journal uses only reserved sectors 16/32, after verification of the GSD1 marker/MBR. No runtime formatting and no out-of-range raw writes are performed.
- The GUP1 signature, image hash and version checks must pass. A wire-format change that includes GJR1 is given a version.
- `UPDATE_PUBLIC_KEY` is a raw public key. A signing private key is never added to Git. The key corresponding to the existing card is used.

## Preparation and transfer

```sh
python3 tools/update/prepare.py --help
python3 tools/update/remote.py --help
make update-test
```

Give `prepare.py` an existing `--key-dir`, a `--version` greater than the current highest, and a new `--output-dir`. `--label` is a short ASCII identifier. The Circle/toolchain paths follow the [setup](pi4-development-setup.md). The app and recovery are built, but this does not mean the recovery is replaced over an online transfer.

Transfer the generated `update.gup` with `remote.py`. On a connection where no TTY is assigned to USB CDC, `--raw-usb` can be used. Before running, finish any recording and file processing, and check the STATUS, version, partition and tryboot recorded by the update tool.

By default, remote leaves the Trial unconfirmed. `--confirm-identity-only` is an option that confirms on identity information alone; it is not a health check of the audio, screens or SD. In addition to the target version and slot being correct, record the check results for the changed functions before confirming. Success or failure is never judged by a timeout alone; the STATUS and the Recovery state are cross-checked.

After confirmation, record the version/highest, phase and error of a normal reboot. Building, signature verification, Trial, confirmation, normal boot and actual-sound checks are recorded separately. For power loss, the design target at each point — during transfer, during writing, with a Trial unconfirmed, and during boot after confirmation — is recovery without bricking. This procedure alone does not prove the safety of WLAN or of all external devices.


## Explicit remote diagnostics

`UITEST` is screen cycling only; `OPTTEST` is an explicit, continuous UI → DSP4 → Fluid qualification. A UI completion marker is never confused with the completion of another diagnostic. `DEPLOYCHECK` is a dedicated functional check: it inspects long-song editing, TWIN, production operations and MODULE/Layer display, and returns to the original Project. While STATUS shows `diagnostic_busy=1`, no other diagnostic, update or transfer can be started. STATUS/LOG can be read.

```sh
python3 tools/testing/deployment_check.py --network DEVICE_IP --version VERSION --command DEPLOYCHECK --output build/deploy-check.json
```

The runner sends the request exactly once, and keeps read observations going through communication delays during processing. It sets a 20-second wait for each individual response and an overall deadline, and checks the completion marker, the release of busy, version/error and uptime separately. This is not proof that panel input physically took effect, nor of how anything sounds.

The working snapshot for update preparation is saved to `GROOVE/Update-Recovery.mgxs`. `PROJECTS/Recovery.mgxs` and the locator of the current song are not save destinations for update preparation.

Observation of `SAVETEST` can also be done with the same runner's `--command SAVETEST`. Busy is judged from the actual asynchronous job, not from the selected save path, and a failed start leaves no permanent busy behind. `--verify-admission` uses the same synchronous client to verify the rejection of ENTER / another diagnostic / USB file OPEN, and requires PyUSB and USB access rights.
