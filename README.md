# GNU-Linux_AudioRTDSP3
Some user-level runtime codes for GNU-Linux based systems. These codes performs Audio Real-Time Digital Signal Processing, more specifically, they sum and subtract the 2 channels of a stereo audio signal.

These codes use headerless (.raw) audio files as an input audio signal. They're meant to be used playing a stereo, 44.1 kHz 16bit audio file. Different audio signal parameters might not work properly on these codes.

These codes require the asoundlib headers and build resources. In Debian based system, these resources can be installed using command "sudo apt-get install libasound2-dev".

When compiling, 2 APIs must be manualy linked: "asound" and "pthread" Example: g++ main.cpp -lpthread -lasound -o executable

I made this code just for fun. I'm not a professional software developer. Don't expect professional performance from it.

Author: Rafael Sabe
Email: rafaelmsabe@gmail.com
