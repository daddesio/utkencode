## Overview

EA MicroTalk (also UTalk or UTK) is a linear-predictive speech codec used in
various games by Electronic Arts. The earliest known game to use it is
Beasts & Bumpkins (1997). The codec has a bandwidth of 11.025kHz (sampling rate
22.05kHz) and frame size of 20ms (432 samples) and only supports mono. It is
typically encoded at 32 kbit/s.

Docs: http://wiki.niotso.org/UTK

In this repository, I have created a set of open source (public domain
via the UNLICENSE) MicroTalk decoders/encoders.

* Use utkdecode to decode Maxis UTK (The Sims Online, SimCity 4).
* Use utkdecode-bnb to decode PT/M10 (Beasts & Bumpkins).
* Use utkdecode-fifa to decode FIFA 2001/2002 (PS2) speech samples. This tool
  supports regular MicroTalk and MicroTalk Revision 3
  [SCxl files](https://wiki.multimedia.cx/index.php/Electronic_Arts_SCxl).(*)
* Use utkencode to encode Maxis UTK. (This is the simplest container format and
  is currently the only one supported for encoding.)

(*) I wasn't able to find any real-world MicroTalk Rev. 3 samples in any games.
However, you can transcode a FIFA MicroTalk Rev. 2 file to Rev. 3 using
[EA's Sound eXchange tool](https://wiki.multimedia.cx/index.php/Electronic_Arts_Sound_eXchange)
(`sx -mt_blk input.dat -=output.dat`).

## Compiling

```
gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math -fwhole-program -g0 -s -static-libgcc -o utkdecode utkdecode.c
gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math -fwhole-program -g0 -s -static-libgcc -o utkdecode-fifa utkdecode-fifa.c
gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math -fwhole-program -g0 -s -static-libgcc -o utkdecode-bnb utkdecode-bnb.c
gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math -fwhole-program -g0 -s -static-libgcc -o utkencode utkencode.c
```

## How the encoder works

The encoder for now is very simple. It does LPC analysis using the Levinson
algorithm and transmits the entire excitation signal explicitly. Compression is
achieved by choosing a large fixed codebook gain, such that each excitation
sample has a large (coarse) quantization step size. Error is minimized in the
excitation domain, and the quality is somewhat poor for bitrates below about
48 kbit/s.

However, MicroTalk is a multi-pulse codec (it is cheap to code long runs of
zeros in the excitation signal). Hence, a much better design (and indeed the
standard practice for multi-pulse speech codecs) is to search for the positions
and amplitudes of n pulses such that error is minimized in the output domain
(or the perceptually weighted domain). This new encoder is still in the works.