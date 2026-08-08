# Bedrock FPS Booster Scaffold

This is a starter native mod scaffold for a stripped Android `libminecraftpe.so` build.

What it does:
- Hooks `eglSwapBuffers`
- Tries to enable AGDK SwappyGL frame pacing when the symbols exist
- Logs basic initialization info
- Leaves gameplay logic untouched

What it does not do:
- It does not patch game internals with hardcoded offsets
- It does not promise a compiled, drop-in `.so` for every Bedrock version
- It does not replace the game renderer with Vulkan

Files:
- `CMakeLists.txt`
- `fps_booster.cpp`

Build note:
- Intended as an Android NDK shared library starter.
- A real release build usually needs an injector / loader that loads this library into the game process.
