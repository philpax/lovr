# <a href="https://lovr.org"><img src="https://lovr.org/static/img/logo.svg" width="28"/></a> LÖVR

> **A simple Lua framework for rapidly building 3D games and VR experiences.**

[![Build](https://github.com/bjornbytes/lovr/actions/workflows/build.yml/badge.svg?event=push)](https://github.com/bjornbytes/lovr/actions/workflows/build.yml)
[![Version](https://img.shields.io/github/release/bjornbytes/lovr.svg?label=version)](https://github.com/bjornbytes/lovr/releases)
[![Matrix](https://img.shields.io/badge/chat-matrix-0ba378.svg)](https://lovr.org/matrix)
[![Discord](https://img.shields.io/badge/chat-discord-404eed.svg)](https://lovr.org/discord)

[**Homepage**](https://lovr.org) | [**Documentation**](https://lovr.org/docs) | [**FAQ**](https://lovr.org/docs/FAQ)

<p align="left">
  <span><img src="http://lovr.org/static/img/screen1.jpg" width="32.5%"/></span>
  <span><img src="http://lovr.org/static/img/screen2.jpg" width="32.5%"/></span>
  <span><img src="http://lovr.org/static/img/screen3.jpg" width="32.5%"/></span>
</p>

Features
---

- **Cross-Platform** - Runs on Windows, macOS, Linux, and Android.
- **Cross-Device** - Supports any headset that works with OpenXR, and has a keyboard/mouse VR simulator.
- **Beginner-friendly** - Simple scenes can be created in just a few lines of Lua.
- **Fast** - Written in C11 and scripted with LuaJIT, includes optimized single-pass stereo rendering.
- **Tiny** - The whole framework is just a couple of megabytes, and it's easy to disable/remove modules.
- **Powerful GPU APIs** - Compute shaders, ray tracing, indirect rendering, atomics, and more.
- **Asset Import** - Supports glTF models, skeletal animation, compressed textures, cubemaps, fonts, etc.
- **3D Rigid Body Physics** - Includes JoltPhysics, with 7 collider shapes and 6 types of joints.
- **Spatialized Audio** - Uses SteamAudio for audio spatialization, with HRTFs, audio meshes, and reverb.
- **Vector Library** - Built-in 3D vectors, quaternions, and matrices.

Getting Started
---

It's really easy to get started making things with LÖVR.  Grab a copy of the executable from <https://lovr.org/downloads>,
then write a `main.lua` script and drag it onto the executable.  Here are some example projects to try:

#### Hello World

```lua
function lovr.draw(pass)
  pass:text('Hello World!', 0, 1.7, -3, .5)
end
```

#### Spinning Cube

```lua
function lovr.draw(pass)
  pass:cube(0, 1.7, -1, .5, lovr.timer.getTime())
end
```

#### Hand Tracking

```lua
function lovr.draw(pass)
  for _, hand in ipairs(lovr.headset.getHands()) do
    pass:sphere(vec3(lovr.headset.getPosition(hand)), .1)
  end
end
```

#### 3D Models

```lua
function lovr.load()
  model = lovr.graphics.newModel('model.gltf')
end

function lovr.draw(pass)
  pass:draw(model, x, y, z)
end
```

More examples are on the [docs page](https://lovr.org/docs/Intro/Hello_World).

Building
---

You can build LÖVR from source using CMake.  Here are the steps using the command line:

```console
mkdir build
cd build
cmake ..
cmake --build .
```

See the [Compiling Guide](https://lovr.org/docs/Compiling) for more info.

The optional Linux Vulkan OpenVR connection foundation builds with `-DLOVR_ENABLE_OPENVR=ON`.
It does not yet provide a selectable headset backend. The option defaults to off.
`deps/openvr` pins Valve's OpenVR SDK 2.15.6 at `0924064316de3effbcd1acf1e309182a2deb1c05`.
The static loader is built from the SDK sources into the CMake build directory, without a SteamVR installation.
The SDK's redistribution terms are in `deps/openvr/LICENSE`; binary distributions that enable the option must include them.
Loading LÖVR does not initialize OpenVR. Connecting resolves the runtime through the SDK's runtime registry.
The private adapter permits one process-wide connection. Its owning object must remain at a stable address
and must not be copied or externally mutated. Loader callbacks must not reenter the adapter.
Callers serialize function-table use against disconnect.

Hardware-free native connection tests build with `-DLOVR_BUILD_HEADSET_TESTS=ON`.
Build the `headset_native_tests` target and run `ctest --test-dir <build-directory> --output-on-failure -L headset`.
`-DLOVR_SANITIZE=ON` instruments the native test executables on supported compilers.

Testing
---

Tests can be found in the `test/lovr` directory.

You can run all of LÖVR tests from the command line with the following:

```console
./build/bin/lovr test
```

To run tests for a single module, pass the name of the module as an argument
(e.g. this runs the tests for the data module):

```console
./build/bin/lovr test data
```

Resources
---

- [**Documentation**](https://lovr.org/docs): Guides, tutorials, examples, and API documentation.
- [**FAQ**](https://lovr.org/docs/FAQ): Frequently Asked Questions.
- [**Matrix**](https://lovr.org/matrix): The LÖVR community for discussion and support.
- [**Nightly Builds**](https://lovr.org/download/nightly): Nightly builds for Windows.
- [**Compiling Guide**](https://lovr.org/docs/Compiling): Information on compiling LÖVR from source.
- [**Contributing**](https://lovr.org/docs/Contributing): Guide for helping out with development 💜
- [**LÖVE**](https://love2d.org): LÖVR is heavily inspired by LÖVE, a 2D game framework.

License
---

MIT, see [`LICENSE`](LICENSE) for details.
