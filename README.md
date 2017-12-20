# BeatFinder
A C++ library implementation of two audio signal reduction methods for analysis and beat detection.

The exposed method `FindBeats` takes a file path as input and transcodes it using `ffmpeg` 3.4.1 before processing audio data. 

Analysis is performed using a 1024-point non-overlapping STFT, with the Hamming window function applied to the samples beforehand. Transcoding is performed frame by frame, such that reduction is possible in real time (currently not supported).

The output struct contains the spectral flux representation of the audio stream, its peaks (with non-peak windows set to 0), as well as the amplitude envelope follower proposed in "A Tutorial on Onset Detection in Music Signals" (Bello et al., 2005).
