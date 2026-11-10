# The dream build: a Pi inside the MK3

> **Under construction — this build has not been done yet.**
> Nobody has measured the inside of the case for this yet, and
> this page will grow from a real teardown, not from guesses.
> Do not improvise power or USB connections inside a MASCHINE
> MK3 based on this page.

The stock setup needs no modification at all: the Pi sits
outside, plugged into the USB port, and unplugging it returns
the MK3 to a completely normal MASCHINE. This page is for the
other kind of person — the one who dreams of a single box with
the computer inside it.

## What has to be true before anyone solders anything

- **Room.** Internal free space and height for a Pi 4, with an
  insulating sheet between it and the MK3's own boards, and
  clearance that survives the case being closed and carried.
- **Power.** The MK3's internal rails must be measured first:
  voltages, and headroom at worst case (full display
  brightness, all pads lit, audio at level). A Pi 4 wants a
  solid 5 V / 3 A-class supply; whether the host can honestly
  provide that is a measurement, not an assumption.
- **USB, internalised.** The Pi↔MK3 USB conversation moves to
  an internal tap at the rear connector: D+/D−/VBUS/GND lands
  identified and photographed on the real unit. Consequence,
  stated plainly: the rear socket is occupied, and the
  "plug it back into a computer" trick is gone.
- **Heat.** A sealed case has no breeze. One hour of continuous
  worst-case playing, temperatures logged, before this can be
  called a build rather than an experiment.
- **The rescue hatch stays.** The microSD slot and the Pi's
  USB-C (power + update/control) must remain reachable without
  opening the case. YADOKARI's update model always leaves
  re-flashing as the owner's escape route — sealing that away
  would seal away the owner's freedom with it.
- **Mounting.** Standoffs or a printed cradle, strain relief on
  every lead, nothing resting on a board it shouldn't.

## Order of work

1. Prototype the whole thing with external cables first.
2. Measure and photograph everything on the real unit.
3. Write the guide from those records — only then does this
   page become instructions.

## The honest warning

Opening the case ends your warranty, and a mistake here can
kill a controller that did nothing wrong. The unmodified
setup loses none of the music. This build is romance, and it
should be done like romance: slowly, and with your eyes open.
