An unimpressive thingie for playing bits of samples with some level of accuracy.

(c) Alex McLean and contributors, 2016
Released under the GNU Public Licence version 3 

# Linux installation

Here's how to install dirt under Debian, Ubuntu or a similar distribution:

~~~~sh
sudo apt-get install \
    build-essential git \
    libsndfile1-dev libsamplerate0-dev liblo-dev \
    libpulse-dev portaudio19-dev libsdl2-dev libjack-jackd2-dev \
    qjackctl jackd
git clone --recursive https://github.com/tidalcycles/Dirt.git
cd Dirt
make clean; make
~~~~

To disable a subset of audio backends at build time:

~~~~sh
make clean; make JACK=0 SDL2=0 PULSE=0 PORTAUDIO=0
~~~~

## Starting Dirt under Linux

The history of Linux audio is complicated.
Therefore there are several ways to get Dirt to talk to your hardware.

### SDL2 backend

SDL2 is a cross-platform framework for realtime multimedia applications
(for example, games).

The SDL2 backend is new in Dirt version 1.1, so it is not as well-tested
as the JACK backend, but it is usually a lot easier to get started with:

~~~~sh
cd ~/Dirt
./dirt -o sdl2
~~~~

### JACK backend

JACK is a sound server designed for professional live and studio use.

First of all, start the "jack" audio layer. The easier way to do this
is with the "qjackctl" app, which you should find in your program
menus under "Sound & Video" or similar. If you have trouble with
qjackctl, you can also try starting jack directly from the
commandline:

~~~~sh
jackd -d alsa &
~~~~

If that doesn't work, you might well have something called
"pulseaudio" in control of your sound. In that case, this should work:

~~~~sh
/usr/bin/pasuspender -- jackd -d alsa &
~~~~

And finally you should be able to start dirt with this:

~~~~sh
cd ~/Dirt
./dirt -o jack &
~~~~

If you have problems with jack, try enabling realtime audio, and
adjusting the settings by installing and using the "qjackctl"
software. Some more info can be found in the [Ubuntu Community page for JACK configuration](https://help.ubuntu.com/community/HowToJACKConfiguration)

### PulseAudio backend

PulseAudio is a sound server designed for desktop use.

~~~~sh
cd ~/Dirt
./dirt -o pulse
~~~~

### PortAudio backend

PortAudio is a cross-platform compatibility layer
for audio applications.

~~~~sh
cd ~/Dirt
./dirt -o portaudio
~~~~

# MacOS installation

Installing Dirt's dependencies on Mac OS X can be done via homebrew or
MacPorts, but choose only one to avoid conflicts with duplicate system
libraries.

Unless otherwise specified, the below commands should be typed or
pasted into a terminal window.

## Installing dependencies via Homebrew

[Homebrew](http://brew.sh) is a package manager for OS X. It lives
side by side with the native libraries and tools that ship with the
operating system.

To install homebrew:

~~~bash
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
~~~

Initialise homebrew:

~~~bash
brew doctor
~~~

Install Dirt, a synth (well, more of a sampler) made to work with
Tidal. A homebrew 'recipe' for dirt does exist, but that doesn't come
with any sounds to play with, so for now it's probably easiest just
download it all from github and compile it as follows.

Install some libraries which the Dirt synth needs to compile:

~~~bash
brew install liblo libsndfile libsamplerate
~~~

Install the 'jack audio connection kit' which Dirt also needs:

~~~bash
brew install jack
~~~

*If Homebrew's installation of Jack fails with a `make` error, you can
 use the [JackOSX Installer](http://www.jackosx.com/download.html)
 instead. This will, however, add an additional step when installing
 Dirt (see below).*


### Alternative: Installing dependencies via Mac Ports

[MacPorts](https://www.macports.org/) is another package manager for
OS X.

If you already installed dependencies via homebrew, skip ahead to build Dirt.  
Otherwise if you happen to already use MacPorts, here's a list of
steps in order to get all dependencies:

~~~bash
sudo port install liblo libsndfile libsamplerate
~~~

Download and install jack2 [Jack Download Page](http://jackaudio.org/downloads/). Jack 2 has better OS X integration [Jack Comparison](https://github.com/jackaudio/jackaudio.github.com/wiki/Q_difference_jack1_jack2).

## Building Dirt from source

Get the source code for the Dirt synth:

~~~bash
cd ~
git clone --recursive https://github.com/tidalcycles/Dirt.git
~~~

Compile dirt:

~~~bash
cd ~/Dirt
make clean; make SDL2=0 PULSE=0 PORTAUDIO=0
~~~

If Dirt fails to compile after using the JackOSX installer as above,
 you may need to add flags to the Makefile to specify the appropriate
 paths:

~~~make
CFLAGS += -g -I/usr/local/include -Wall -O3 -std=gnu99 -DCHANNELS=2
LDFLAGS += -lm -L/usr/local/lib -llo -lsndfile -lsamplerate -ljack
~~~

### Homebrew users

As MacPorts installs all libs on `/opt/local/`
edit the Makefile to point the right direction of `libsndfile` and `libsamplerate`

~~~make
CFLAGS += -g -I/opt/local/include -Wall -O3 -std=gnu99
LDFLAGS += -lm -L/opt/local/lib  -llo -lsndfile -lsamplerate
~~~

## Starting Dirt under MacOS

To start Dirt, back in a terminal window, first start jack:

~~~bash
jackd -d coreaudio &
~~~

Or, if you downloaded Jack 2, then start the JackPilot at:
/Applications/Jack/JackPilot.app

Click __start__ button.

Then start dirt:

~~~bash
cd ~/Dirt
./dirt &
~~~

# Windows installation

## Building on Windows using Cygwin

### Cygwin

First, install [Cygwin](https://www.cygwin.com). In Cygwin, make sure the
following packages are installed:

~~~~
git
gcc-core
make
gcc-g++
libsndfile
libsndfile-devel
libsamplerate
libsamplerate-devel
~~~~

### Portaudio

Download Portaudio from http://www.portaudio.com. In Cygwin, Unpack
the download with `tar fxvz`. After unpacking, from Cygwin, go to the directory
where you unpacked Portaudio and then run:

~~~~bash
./configure && make && make install
~~~~

### Liblo

Download [Liblo](http://liblo.sourceforge.net).
In Cygwin, unpack Liblo with `tar fxvz`, then in Cygwin go to the directory where you
unpacked Liblo and then run:

~~~~bash
./configure && make && make install
~~~~

### Dirt

In Cygwin:

~~~~bash
git clone --recursive http://github.com/tidalcycles/Dirt.git
~~~~

Then:

~~~~bash
cd Dirt
make dirt-pa
~~~~

Then you get a `dirt-pa.exe` that works. Maybe this even works on any
windows system without having to compile. You'd need `cygwin1.dll` at
least though.

## Building for Windows from Linux

These instructions are for `x86_64` (aka `x64`, `amd64`)
CPU architecture, which is most Windows PCs (64bit).
ARM is starting to appear on some new laptops, and
some old systems might still be 32bit (aka `x86`, `i686`).
Adapting these instructions to other architectures is possible,
see <https://mathr.co.uk/web/build-scripts.html#Prerequisites>
for inspiration.

### MINGW

You need the MINGW toolchain to cross-compile for Windows.

~~~~sh
sudo apt-get install mingw-w64
~~~~

You might also want to get Wine to test Windows programs on Linux.

~~~~sh
sudo apt-get install wine
~~~~

### Dependencies

Use `build-scripts` to install
`lo`, `samplerate`, `sndfile`, `portaudio` and `sdl2`
for Windows:

~~~~sh
git clone https://code.mathr.co.uk/build-scripts.git
cd build-scripts
for arch in download x86_64-w64-mingw32
do
  ./BUILD.sh "${arch}" "lo samplerate sndfile portaudio sdl2"
done
~~~~

The sources are downloaded to `${HOME}/opt/src`
and compiled output will be in `${HOME}/opt/windows/posix`.

You can omit `portaudio` if you add `PORTAUDIO=0` to the `make` command.

### Building Dirt

~~~~sh
make WINDOWS=1
~~~~

You need to run `make clean` between non-Windows and Windows builds.

### Testing Dirt

~~~~sh
wine dirt.exe
wine dirt-gui.exe
~~~~

Some versions of Wine show console output, some don't
(matching Microsoft Windows).
The GUI version displays messages in a window.

# Android

## Building

If you're not hacking on Dirt's code, you can skip this section
and follow the next one.

### SDK + NDK

You need Android SDK + NDK in `${ANDROID_HOME}` and `${ANDROID_NDK_HOME}`,
for setup on Debian Linux see:
<https://mathr.co.uk/web/build-scripts.html#SDK-and-NDK>.

You also need a fair amount of free disk space.

### Dependencies

Use `build-scripts` to install `lo`, `samplerate`, and `sndfile` for Android:

~~~~bash
git clone https://code.mathr.co.uk/build-scripts.git
cd build-scripts
for arch in download aarch64-linux-android21 armv7a-linux-androideabi21 i686-linux-android21 x86_64-linux-android21
do
  ./BUILD.sh "${arch}" "lo samplerate sndfile"
done
~~~~

The sources are downloaded to `${HOME}/opt/src`
and compiled output will be in `${HOME}/opt/android/21`.

### Dear ImGui

Dirt for Android uses Dear ImGui for user interface,
the cloned respository is expected to be
next to the Dirt folder.  For example:

~~~~bash
git clone https://github.com/ocornut/imgui.git
git clone https://github.com/tidalcycles/Dirt.git
~~~~

### Building Dirt for Android

First you need to choose the package name of the app:

~~~~bash
export DIRT_PACKAGE=name.domain.your.dirt.v1
~~~~

A good choice is your own domain name
with components in reverse order,
plus the name of the app, and a version tag.
It isn't preset, so that different developers can make
their own versions that can be installed in parallel.

Dirt for Android uses SDL2 as application wrapper,
to download SDL2 and set up the app do:

~~~~bash
cd Dirt
./android.sh prepare ${DIRT_PACKAGE}
~~~~

Then to do a debug build and install on a connected device:

~~~~bash
./android.sh debug ${DIRT_PACKAGE}
~~~~

To do a release build (faster, requires signing):

~~~~bash
./android.sh release ${DIRT_PACKAGE}
~~~~

It will be installed on a connected device and
you should also end up with a
`${DIRT_PACKAGE}-${VERSION}.apk`
file in the main Dirt directory.

## Installing

If you're hacking on Dirt's code, you can skip this section
and follow the previous one.

There is no official Dirt APK yet.

You can get an unofficial one from
`https://mathr.co.uk/web/dirt.html#Android`.

## Samples

Dirt for Android doesn't come with any samples.
You need to install them separately.
Depending on Android version, permissions, etc,
you might have to put them internal storage and SD card won't work.

Samples are expected to be in
`Storage` / `Android` / `data` / `${DIRT_PACKAGE}` / `files` / `samples` / `${SAMPLE_NAME}` / `${SAMPLE_FILE}.wav`.

To get the default Dirt samples follow these steps
(maybe you are lucky and you can replace Internal Storage with SD):

1. On your Android device, download `Dirt-Samples` as a zip archive:
   <https://github.com/tidalcycles/Dirt-Samples/archive/refs/heads/master.zip> (170MB).

2. Move the zip to `Internal Storage` / `Android` / `data` / `${DIRT_PACKAGE}` / `files`.

3. Unzip the zip (this will take up another 225MB).

4. If your unzipper helpfully made an extra directory, like `master/Dirt-Samples-master`,
   move the `Dirt-Samples-master` directory to be inside the parent directory `files`.

5. Rename the `Dirt-Samples-master` directory to `samples`.

6. You should end up `...` / `${DIRT_PACKAGE}` / `files` / `samples`,
   with many folders inside, each containing WAV sample files.

7. You can now delete the zip archive to save space.

## Launch

Launch Dirt as you would any other app.
You will see a configuration screen that lets you choose some settings,
defaults are usually fine.
Then activate the checkbox to the left of the start button to unlock it,
and press the start button to start the Dirt engine.

There is no elegant way to stop or restart the Dirt engine yet,
use normal Android method force stop for now
(e.g. on the open apps list, swipe the Dirt app off the screen).

If the log gets too full, you can clear it:
activate the checkbox to the left of the clear button to unlock it,
and press the clear button to clear the log.

Dirt for Android tries to keep running in the background,
but it can sometimes be stopped by the system if you
open other apps that demand too many resources.

## Make Noise

Send OSC to Dirt from your favourite tools as usual,
perhaps some commmand line magic
running in Debian inside the UserLAnd app
installed from f-Droid.
