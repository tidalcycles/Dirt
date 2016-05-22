An unimpressive thingie for playing bits of samples with some level of accuracy.

(c) Alex McLean and contributors, 2016
Released under the GNU Public Licence version 3 

# Linux installation

Here's how to install dirt under Debian, Ubuntu or a similar distribution:

~~~~sh
sudo apt-get install build-essential libsndfile1-dev libsamplerate0-dev \
                     liblo-dev libjack-jackd2-dev qjackctl jackd git
git clone --recursive https://github.com/tidalcycles/Dirt.git
cd Dirt
make clean; make
~~~~

## Starting Dirt under Linux

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
./dirt &
~~~~

If you have problems with jack, try enabling realtime audio, and
adjusting the settings by installing and using the "qjackctl"
software. Some more info can be found in the [Ubuntu Community page for JACK configuration](https://help.ubuntu.com/community/HowToJACKConfiguration)

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
make clean; make
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

## Cygwin

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

## Portaudio

Download Portaudio from http://www.portaudio.com. In Cygwin, Unpack
the download with `tar fxvz`. After unpacking, from Cygwin, go to the directory
where you unpacked Portaudio and then run:

~~~~bash
./configure && make && make install
~~~~

## Liblo

Download [Liblo](http://liblo.sourceforge.net).
In Cygwin, unpack Liblo with `tar fxvz`, then in Cygwin go to the directory where you
unpacked Liblo and then run:

~~~~bash
./configure && make && make install
~~~~

## Dirt

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

