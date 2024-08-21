# fmod_wav_out
Simple fmod output plugin which allows writing to custom files to do synced offline rendering for trailer videos etc.

## Build
- fmod headers need to go in the `./fmod` folder (like `./fmod/fmod.hpp` etc.).
- fmod binaries need to in `./lib` (like `./lib/libfmod.so`, `./lib/fmod_vc.lib`)
- `mkdir build`
- `cd build`
- `cmake ..`
- `cmake --build .` or open .sln

## Usage
- This plugin is not intended to be loaded via `FMOD::System::loadPlugin()`! (Easy to change though)
- Call `fmod_wav_out_register(FMOD::System* system)` with the fmod system instance.
  - Will register the output plugin to fmod
  - Change the fmod block size `FMOD::System::setDSPBufferSize` to ensure each update produces the exact amount of audio samples needed for the (hardcoded) framerate `fmod_wav_out_fps`
  - Will return a output plugin handle on success.
- Call `FMOD::System::setOutputByPlugin` with the handle returned above.
  - I haven't changed the output after `FMOD:Studio.init`, there might be some problems.
- Call `FMOD::System.init` or `FMOD:Studio.init` with theses flags to ensure everything runs properly in non realtime.
  - `FMOD::System`
    - `FMOD_INIT_MIX_FROM_UPDATE` each `FMOD::System.update` call will directly call `fmod_wav_upate`
    - `FMOD_INIT_STREAM_FROM_UPDATE` otherwise streaming samples might run out when rendering at high frame rates.
    - `FMOD_INIT_THREAD_UNSAFE` optional, but the init flags above get rid of any threading.
  - `FMOD::Studio` not tested, but these should probably be set when using Studio.
    - `FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE`
    - `FMOD_STUDIO_INIT_LOAD_FROM_UPDATE`
- Get the internal plugin state handle after fmod is initialized with `FMOD::System.getOutputHandle`.
  - Not to be confused with the handle returned from `fmod_wav_out_register`
  - This handle only points to the correct data after `FMOD::System::setOutputByPlugin` was called!
  - It points to `fmod_wav_out_state` and this state is needed for the other functions below.
- `fmod_wav_out_prepare_recording(fmod_wav_out_state* state, const char* path)` Opens the file handle to the provided path (e.g. `/tmp/output.wav`)
- `fmod_wav_out_start_recording(fmod_wav_out_state* state)` will start writing audio to the prepared output file on the next `FMOD::System.update` or `FMOD::Studio.update` call.
- Keep calling `FMOD::System.update` or `FMOD::Studio.update`
- `fmod_wav_out_stop_recording(fmod_wav_out_state* state)` stops recording and closes the output file.
- Repeat if needed.


## P/Invoke for c#

```cs
[DllImport("fmod_wav_out", CallingConvention = CallingConvention.Cdecl)]
public static extern uint fmod_wav_out_register(IntPtr systemHandle);

[DllImport("fmod_wav_out", CallingConvention = CallingConvention.Cdecl)]
public static extern bool fmod_wav_out_stop_recording(IntPtr pluginHandle);

[DllImport("fmod_wav_out", CallingConvention = CallingConvention.Cdecl)]
public static extern bool fmod_wav_out_prepare_recording(IntPtr pluginHandle, IntPtr path);

[DllImport("fmod_wav_out", CallingConvention = CallingConvention.Cdecl)]
public static extern bool fmod_wav_out_start_recording(IntPtr pluginHandle);
```