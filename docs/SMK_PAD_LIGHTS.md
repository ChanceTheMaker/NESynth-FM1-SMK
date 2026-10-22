# SMK pad-light candidate 971

The owner reported first-touch dimming and incorrect colors when moving Fader 2
on GameDude 952. This firmware used the same LED encoder and transport headers.
Candidate 971 adopts the shared fix tested in Doom 24 and GameDude 953:
512 zero bytes before and after the color data, inside the DMA transfer. GPIO
pauses outside a transfer do not by themselves guarantee a low signal while
SPI owns the pin.

Control mapping, cyan/magenta colors, startup brightness 150, Fader 2 behavior,
FM-1 code path and game-library layout remain unchanged. Read-only SMK USB
status command 7 reports brightness, raw fader input, held pads, last light
state, write count and transfer errors.

Host sanitizer checks pass for all 256 brightness levels, all 16 held-pad
positions, reset framing, DMA ordering, display restoration, timeout cleanup,
brightness behavior and control mapping. The owner confirmed GameDude 953's
colors stay correct and brightness works. This project's hardware status is
recorded below; that confirmation does not establish its physical behavior.
Public site and release manifests remain unchanged.

The complete candidate builds and passes RAM/application bounds, protected-byte
package round-trip, and simulated installation of the actual packet.
Application SHA-256: `232364b0b828851a17881a4da729f98e3488fdf280fcea88e977364d9c0758c8`.
Application bytes: 208260; free application space: 8540.

Hardware installation from Color 985 completed with exact SMK-37_971 restart.
The bundled From Below cartridge (40,976 bytes) uploaded over USB in 1.06
seconds with CRC 8ce49adb verified. The owner confirmed the NESynth fader/pad check: "Looks good".

After the owner check, NESynth telemetry showed 106 completed light updates,
brightness 185 at raw Fader 2 741..742, and zero LED transfer errors. The
verified cartridge remained active with loader error 0 and radio off. This
sample followed the interaction; it does not capture the full fader sweep.

## Public release 971

The accepted application above is now selected by the public installer and
matching Windows USB helper. FM-1 firmware is unchanged. Original-firmware
restore and its existing compatibility checks are retained. The release uses
the exact physically tested application, with no rebuild or private calibration.

Release validation: both Pro and Elite public packages pass model/layout,
checksum, corruption and wrong-model rejection checks. The native Pro install
package matches the physically installed image and packet byte for byte.
The original-firmware package builds successfully. Isolated headless Chrome
checks pass install and restore with simulated USB, verify the Windows helper
path does not open Web MIDI, and preserve the FM-1 browser path. These checks
do not replace physical Elite or a new power-cycle/stock round-trip test.
