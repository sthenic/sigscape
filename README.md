[![LICENSE](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](https://opensource.org/licenses/MIT)
![build](https://github.com/sthenic/sigscape/workflows/build/badge.svg)

# sigscape

This application is a minimal but feature rich GUI for ADQ3 series digitizers
from [Teledyne SP Devices](https://spdevices.com).

**This project is not affiliated with Teledyne SP Devices.** It's a passion
project I've developed to demonstrate the flexibility afforded to a user
application by the digitizer's API.

![ui](data/ui.png)

## Index

- [Core concepts](#core-concepts)
- [Persistent data](#persistent-data)
- [Embedded Python](#embedded-python)
- [Building](#building)
- [License](#license)
- [Third-party dependencies](#third-party-dependencies)

## Core Concepts

### Configuration

Parameters are set via JSON files which you read and edit using your favorite
text editor. These files are generated and parsed by the digitizer's API (not
this application), leveraging the first-class support provided for this format.

The main benefit of this is an application that will hopefully hold up well over
time. When new features are added to the ADQ3 series digitizers, or existing
features change, this application will keep on working with minimal to no
intervention needed.

**There will be never be GUI elements to control the parameters of the digitizer
itself.** This obviously trades off some ease-of-use to create a GUI that's
cheap to maintain. This is by design.

### Multi-threaded Processing

The fact that the API calls used during the data acquisition phase are
thread-safe is leveraged to create a performant application with individual
data processing threads for each channel.

Running this application on CPUs with a low number of cores is not recommended.

## Persistent Data

When the application is launched, the system is searched for a place to store
persistent files. These files are separated into two categories: *configuration*
and *data*. *Configuration files* are typically the digitizers' JSON parameter
files. What constitutes a *data file* is more loosely defined. Currently the
definition includes screenshots, digitizer trace logs and Python scripts used to
define [custom commands](#embedded-python). The rules for locating these
*persistent directories* are platform dependent.

On Windows, configuration files are stored in `%APPDATA%/sigscape/config` and
data files are stored in `%APPDATA%/sigscape/share`.

On Linux, configuration files are stored in

- `$XDG_CONFIG_HOME/sigscape`, if the environment variable `XDG_CONFIG_HOME` is
  set; and
- `$HOME/.config/sigscape` otherwise.

Data files are stored in

- `$XDG_DATA_HOME/sigscape`, if the environment variable `XDG_DATA_HOME` is set;
  and
- `$HOME/.local/share/sigscape` otherwise.

## Embedded Python

You can extend the command palette with your own custom commands. These are
defined in Python and utilizes the `pyadq` package to control the digitizer.

Python scripts that define custom commands *must*

- be located in `<persistent_data>/python`; *and*
- define `main` as a callable function taking exactly one argument: the
  digitizer as a `pyadq.ADQ` object.

The filename (stem) will be used to identify the command in the UI. The script
directory is continuously monitored so there is no need to restart the
application to synchronize changes.

As an example, consider the following script that simply calls `ADQ_Blink()` for
the target digitizer, causing it to slowly blink with a designated LED in the
front panel.

```python
import pyadq

def main(digitizer: pyadq.ADQ):
    print(f"Blinking with {digitizer}")
    digitizer.ADQ_Blink()
```

The streams `stdout` and `stderr` are captured each time a script is called and
presented in the application log (unless they're empty).

### Environment

It is important to note that `sigscape` does not bundle a Python environment,
but instead makes use of the one available on the system. This is so you have
access to the same environment and all your packages that you're used to.

**`sigscape` hooks into the global Python environment. Virtual environments are
not supported.**

On Linux, the Python environment is often well-defined and managed by the
distribution's package manager. When `sigscape` is compiled, the shared library
for Python will be dynamically linked to the executable. Thus, if this library
is missing from the system where `sigscape` is launched, there will be a runtime
error.

On Windows, the location of the Python environment is more loosely defined. For
this reason, `sigscape` uses runtime dynamic linking to still allow the
application to start even if a Python environment cannot be found on the system.
The location of the Python runtime must indexed by the system's `PATH`;
specifically `python3.dll`, which contains the ABI-stable C-API. A
well-provisioned Python distribution normally contains this file together with
the version-specific shared library, e.g. `python310.dll` for Python 3.10. The
official package from https://python.org/ provides such an environment.
**Remember to add the installation directory to the `PATH` when prompted.**

There will be a message in the application log indicating whether or not the
embedded Python environment could be initialized.

## Building

### Linux

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build
```

On Ubuntu 22.04 the following libraries are needed:
```
apt install libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libudev-dev
```

### Windows

```
cmake -B build -A x64 -S .
cmake --build build --config Release
```

## License

This application is free software released under the [MIT
license](https://opensource.org/licenses/MIT).

## Third-party dependencies

See [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md)
