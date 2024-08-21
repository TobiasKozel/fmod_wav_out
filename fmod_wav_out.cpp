#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cassert>

#include "fmod.hpp"

#define DR_WAV_IMPLEMENTATION
#include "./dr_wav.h"

FMOD_OUTPUT_DESCRIPTION fmod_wav_plugin_description;

FMOD_RESULT F_CALLBACK fmod_wav_get_num_drivers(
	FMOD_OUTPUT_STATE* output_state, int* numdrivers);
FMOD_RESULT F_CALLBACK fmod_wav_get_driver_info(
	FMOD_OUTPUT_STATE* output_state, int id, char* name, int namelen, FMOD_GUID* guid,
	int* systemrate, FMOD_SPEAKERMODE* speakermode, int* speakermodechannels);
FMOD_RESULT F_CALLBACK fmod_wav_init(
	FMOD_OUTPUT_STATE* output_state, int selecteddriver, FMOD_INITFLAGS flags, int* outputrate,
	FMOD_SPEAKERMODE* speakermode, int* speakermodechannels, FMOD_SOUND_FORMAT* outputformat, int dspbufferlength,
	int* dspnumbuffers, int* dspnumadditionalbuffers, void* extradriverdata);
FMOD_RESULT F_CALLBACK fmod_wav_close(
	FMOD_OUTPUT_STATE* output_state);
FMOD_RESULT F_CALLBACK fmod_wav_upate(
	FMOD_OUTPUT_STATE* output_state);
FMOD_RESULT F_CALLBACK fmod_out_get_handle(
	FMOD_OUTPUT_STATE* output_state, void** handle);

constexpr int fmod_wav_out_channels = 2;
constexpr int fmod_wav_out_sample_rate = 48000;
constexpr int fmod_wav_out_fps = 60;
constexpr int fmod_wav_out_block_size = fmod_wav_out_sample_rate / fmod_wav_out_fps;

enum class fmod_wav_out_recording_state {
	stopped = 0,
	prepared,
	waiting_to_record,
	recording,
	waiting_to_stop
};

struct fmod_wav_out_state {
	drwav encoder;
	fmod_wav_out_recording_state recorder_state;
	int fmod_block_size; // We're forced to mix with this once it's set, otherwise fmod will throw a cryptic error
	float interleaved_audio[fmod_wav_out_block_size * fmod_wav_out_channels];
};

#ifdef __cplusplus
extern "C" {
#endif
	F_EXPORT unsigned int F_CALL fmod_wav_out_register(FMOD::System* system) {
		memset(&fmod_wav_plugin_description, 0, sizeof(FMOD_OUTPUT_DESCRIPTION));
		fmod_wav_plugin_description.apiversion = FMOD_OUTPUT_PLUGIN_VERSION;
		fmod_wav_plugin_description.name = "fmod_wav_out";
		fmod_wav_plugin_description.version = 0x00010000;
		fmod_wav_plugin_description.method = FMOD_OUTPUT_METHOD_MIX_DIRECT;
		fmod_wav_plugin_description.getnumdrivers = fmod_wav_get_num_drivers;
		fmod_wav_plugin_description.getdriverinfo = fmod_wav_get_driver_info;
		fmod_wav_plugin_description.init = fmod_wav_init;
		fmod_wav_plugin_description.close = fmod_wav_close;
		fmod_wav_plugin_description.update = fmod_wav_upate;
		fmod_wav_plugin_description.gethandle = fmod_out_get_handle;

		unsigned int handle = 0;
		auto result = system->registerOutput(&fmod_wav_plugin_description, &handle);

		if (result != FMOD_OK) { return 0; }

		result = system->setDSPBufferSize(fmod_wav_out_block_size, 2);

		if (result != FMOD_OK) { return 0; }

		return handle;
	}

	F_EXPORT bool F_CALL fmod_wav_out_stop_recording(fmod_wav_out_state* state) {
		if (state->recorder_state == fmod_wav_out_recording_state::stopped ||
			state->recorder_state == fmod_wav_out_recording_state::waiting_to_stop ||
			state->recorder_state == fmod_wav_out_recording_state::prepared
		) { return true; }

		state->recorder_state = fmod_wav_out_recording_state::waiting_to_stop;
		//while (state->recorder_state != fmod_wav_out_recording_state::stopped) {
		//	since it's all sync we don't need to wait for it to stop
		//}
		auto success = drwav_uninit(&state->encoder) == DRWAV_SUCCESS;
		if (success) {
			state->recorder_state = fmod_wav_out_recording_state::stopped;
			return true;
		}
		return false;
	}

	F_EXPORT bool F_CALL fmod_wav_out_prepare_recording(fmod_wav_out_state* state, const char* path) {
		if (state->recorder_state == fmod_wav_out_recording_state::recording) {
			return false;
		}

		memset(state->interleaved_audio, 0, sizeof(state->interleaved_audio));
		drwav_data_format format;
		format.container = drwav_container_riff;
		format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
		format.channels = fmod_wav_out_channels;
		format.sampleRate = fmod_wav_out_sample_rate;
		format.bitsPerSample = 32;
		auto result = drwav_init_file_write(&state->encoder, path, &format, NULL);
		if (result) {
			state->recorder_state = fmod_wav_out_recording_state::prepared;
			return true;
		}
		return false;
	}

	F_EXPORT bool F_CALL fmod_wav_out_start_recording(fmod_wav_out_state* state) {
		if (state->recorder_state == fmod_wav_out_recording_state::prepared) {
			state->recorder_state = fmod_wav_out_recording_state::waiting_to_record;
			return true;
		}
		assert(false && "Only call when recorder is prepared.");
		return false;
	}

#ifdef __cplusplus
}
#endif


FMOD_RESULT F_CALLBACK fmod_wav_get_num_drivers(FMOD_OUTPUT_STATE* state, int* numdrivers) {
	*numdrivers = 1;
	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK fmod_wav_get_driver_info(
	FMOD_OUTPUT_STATE* state, int id, char* name, int namelen, FMOD_GUID* guid,
	int* system_rate, FMOD_SPEAKERMODE* speakermode, int* speakermodechannels) {
	strncpy(name, "fmod_wav_out", namelen);
	static_assert(fmod_wav_out_channels == 2, "Only stereo for now");
	*speakermode = FMOD_SPEAKERMODE_STEREO;
	*speakermodechannels = fmod_wav_out_channels;
	*system_rate = fmod_wav_out_sample_rate;
	return FMOD_OK;
}


FMOD_RESULT F_CALLBACK fmod_wav_init(
	FMOD_OUTPUT_STATE* output_state, int selected_driver, FMOD_INITFLAGS flags, int* output_rate,
	FMOD_SPEAKERMODE* speaker_mode, int* speaker_mode_channels, FMOD_SOUND_FORMAT* output_format,
	int dsp_buffer_length, int* dsp_num_buffers, int* dsp_num_additional_buffers, void* extra_driver_data
) {
	auto plugin_state = (fmod_wav_out_state*)output_state->alloc(
		sizeof(fmod_wav_out_state), 0, __FILE__, __LINE__);
	memset(plugin_state, 0, sizeof(fmod_wav_out_state));

	if (plugin_state == nullptr)
	{
		return FMOD_ERR_MEMORY;
	}
	output_state->plugindata = plugin_state;
	plugin_state->fmod_block_size = dsp_buffer_length;

	*output_format = FMOD_SOUND_FORMAT_PCMFLOAT;
	*speaker_mode = FMOD_SPEAKERMODE_STEREO;
	static_assert(fmod_wav_out_channels == 2, "Only stereo for now");
	*speaker_mode_channels = fmod_wav_out_channels;
	*output_rate = fmod_wav_out_sample_rate;
	//*dsp_num_buffers = 2;
	//*dsp_num_additional_buffers = 0;

	return FMOD_OK;
}


FMOD_RESULT F_CALLBACK fmod_wav_close(FMOD_OUTPUT_STATE* output_state) {
	auto state = (fmod_wav_out_state*)output_state->plugindata;
	fmod_wav_out_stop_recording(state);
	output_state->free(output_state->plugindata, __FILE__, __LINE__);
	if (state) {
		return FMOD_OK;
	}

	return FMOD_ERR_PLUGIN;
}


FMOD_RESULT F_CALLBACK fmod_wav_upate(FMOD_OUTPUT_STATE* output_state) {
	auto state = (fmod_wav_out_state*)output_state->plugindata;

	// We're mixing audio even if not recording so already playing samples and reverb tails etc.
	// are present from the first block of recording.
	auto result = output_state->readfrommixer(
		output_state, state->interleaved_audio, state->fmod_block_size);

	assert(result == FMOD_OK && "Mixing failed, block size might be wrong.");

	if (state->recorder_state == fmod_wav_out_recording_state::waiting_to_record) {
		state->recorder_state = fmod_wav_out_recording_state::recording;
	}
	if (state->recorder_state == fmod_wav_out_recording_state::recording) {
		auto samples_encoded = drwav_write_pcm_frames(
			&state->encoder, fmod_wav_out_block_size, state->interleaved_audio);

		assert(samples_encoded == fmod_wav_out_block_size && "Encoder has written wrong amount of samples.");
	}
	else if (state->recorder_state == fmod_wav_out_recording_state::waiting_to_stop) {
		state->recorder_state = fmod_wav_out_recording_state::stopped;
	}
	return FMOD_OK;
}


FMOD_RESULT F_CALLBACK fmod_out_get_handle(FMOD_OUTPUT_STATE* output_state, void** handle) {
	*handle = output_state->plugindata;
	return FMOD_OK;
}
