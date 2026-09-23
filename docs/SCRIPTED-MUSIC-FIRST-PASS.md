# Scripted music first pass

The installed `music.vpp` has 107 RIFF/WAVE tracks with the same stereo Microsoft ADPCM profile: 22,050 Hz, 1,024 bytes and 1,012 decoded frames per block. `rf_music_stream` keeps one decoded block and reads the archive incrementally, using less than 5 KiB for its state. It mixes into the common 48 kHz PC/Xbox audio frame after ordinary sound effects. `Music_Start` replaces the active track; `Music_Stop` fades it over authored `values[0]` seconds. A missing or failed track does not stop gameplay.

The L1S1 UID 9911 process-local replay starts `Tribes_Ext.wav`; 120 frames finish with one active stream and a PCM hash different from the no-music control. A second replay starts UID 9911 and fires `Music_Stop` UID 9910 at frame 60; after 360 frames the four-second fade has ended and the stream is inactive. The focused decoder test matches selected native-rate decoded stereo samples from independent FFmpeg 8.1.1 output. PC and NXDK builds pass. Native playback, timing and audio quality remain unverified because the existing XEMU session was left untouched.

The stream is intentionally one voice and does not yet reproduce original playlist transitions, crossfades or checkpoint continuation. The complete 200 MiB source archive is staged as disc data only, never held in Xbox RAM.
