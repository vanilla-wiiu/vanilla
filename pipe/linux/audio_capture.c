#include "audio_capture.h"
#include <alsa/asoundlib.h>
#include <stdio.h>

static snd_pcm_t *capture_handle;

int audio_capture_init() {
    int err;
    
    // Open PCM device for recording
    if ((err = snd_pcm_open(&capture_handle, "default", 
                           SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "Cannot open audio device: %s\n",
                snd_strerror(err));
        return -1;
    }

    // Set parameters
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    
    // Set access type
    snd_pcm_hw_params_set_access(capture_handle, hw_params,
                                SND_PCM_ACCESS_RW_INTERLEAVED);
    
    // Set sample format
    snd_pcm_hw_params_set_format(capture_handle, hw_params,
                                SND_PCM_FORMAT_S16_LE);
    
    // Set sample rate
    unsigned int rate = 48000;
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params,
                                   &rate, 0);
    
    // Set channel count
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, 1);
    
    // Apply parameters
    if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
        fprintf(stderr, "Cannot set parameters: %s\n",
                snd_strerror(err));
        return -1;
    }
    
    // Prepare interface for use
    if ((err = snd_pcm_prepare(capture_handle)) < 0) {
        fprintf(stderr, "Cannot prepare audio interface: %s\n",
                snd_strerror(err));
        return -1;
    }

    return 0;
}

int audio_capture_read(int16_t* buffer, size_t buffer_size) {
    int err;
    
    if ((err = snd_pcm_readi(capture_handle, buffer, buffer_size/2)) != buffer_size/2) {
        if (err < 0) {
            fprintf(stderr, "Read from audio interface failed: %s\n",
                    snd_strerror(err));
            return err;
        }
    }
    
    return err * 2; // Return bytes read
}

void audio_capture_cleanup() {
    if (capture_handle) {
        snd_pcm_close(capture_handle);
        capture_handle = NULL;
    }
} 