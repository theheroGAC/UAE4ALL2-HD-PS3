#include "disk_sound.h"
#include "cdrom.h"
#ifdef __PSP2__
#include <vorbis/vorbisfile.h>
#else
extern "C" {
int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output);
}
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static volatile int disk_sound_volume = 35;
static volatile int pending_floppy_step;
static volatile int pending_floppy_motor;
static volatile int pending_floppy_read;
static volatile int pending_floppy_write;
static volatile int pending_hard_read;
static volatile int pending_hard_write;

static uae_s16 *floppy_samples;
static uae_s16 *hard_samples;
static int floppy_frames;
static int hard_frames;
static int floppy_rate;
static int hard_rate;

static uae_s16 *active_samples;
static int active_frames;
static int active_rate;
static uae_u64 active_position;
static uae_u64 active_end;
static uae_u64 active_step;
static int active_kind;
static int active_remaining;
static int active_length;
static int active_frequency;
static int active_volume;
static uae_u32 active_phase;

#ifdef __PSP2__
static int decode_ogg(const char *path, uae_s16 **samples, int *frames, int *rate)
{
    OggVorbis_File file;
    vorbis_info *info;
    uae_s16 *output = NULL;
    int output_frames = 0;
    int capacity = 0;
    int result;
    int source_rate;

    if (ov_fopen(path, &file) < 0) return 0;
    info = ov_info(&file, -1);
    if (!info || info->channels <= 0 || info->rate <= 0) {
        ov_clear(&file);
        return 0;
    }
    source_rate = (int)info->rate;

    for (;;) {
        char buffer[8192];
        int bitstream = 0;
        long bytes = ov_read(&file, buffer, sizeof(buffer), 0, 2, 1, &bitstream);
        int input_frames;
        int i;
        uae_s16 *expanded;

        if (bytes <= 0) break;
        input_frames = (int)bytes / (2 * info->channels);
        if (output_frames + input_frames > capacity) {
            capacity = capacity ? capacity * 2 : 4096;
            while (capacity < output_frames + input_frames) capacity *= 2;
            expanded = (uae_s16 *)realloc(output, capacity * sizeof(uae_s16));
            if (!expanded) {
                free(output);
                ov_clear(&file);
                return 0;
            }
            output = expanded;
        }
        for (i = 0; i < input_frames; i++) {
            int channel;
            int sum = 0;
            const uae_s16 *input = (const uae_s16 *)buffer;
            for (channel = 0; channel < info->channels; channel++)
                sum += input[i * info->channels + channel];
            output[output_frames + i] = (uae_s16)(sum / info->channels);
        }
        output_frames += input_frames;
    }

    result = ov_clear(&file);
    if (result < 0 || output_frames <= 0) {
        free(output);
        return 0;
    }
    *samples = output;
    *frames = output_frames;
    *rate = source_rate;
    return 1;
}

static int decode_path(const char *name, uae_s16 **samples, int *frames, int *rate)
{
    char path[256];
    snprintf(path, sizeof(path), "app0:/data/sounds/%s", name);
    if (decode_ogg(path, samples, frames, rate)) return 1;
    snprintf(path, sizeof(path), "ux0:/data/uae4all/sounds/%s", name);
    return decode_ogg(path, samples, frames, rate);
}
#else
static int decode_ogg(const char *path, uae_s16 **samples, int *frames, int *rate)
{
    int channels = 0;
    int sample_rate = 0;
    short *output = NULL;
    int num_samples = stb_vorbis_decode_filename(path, &channels, &sample_rate, &output);
    if (num_samples <= 0 || !output || channels <= 0 || sample_rate <= 0) {
        if (output) free(output);
        return 0;
    }
    if (channels == 1) {
        *samples = (uae_s16 *)output;
        *frames = num_samples;
        *rate = sample_rate;
        return 1;
    }
    uae_s16 *mono = (uae_s16 *)malloc(num_samples * sizeof(uae_s16));
    if (!mono) {
        free(output);
        return 0;
    }
    for (int i = 0; i < num_samples; i++) {
        int sum = 0;
        for (int c = 0; c < channels; c++) {
            sum += output[i * channels + c];
        }
        mono[i] = (uae_s16)(sum / channels);
    }
    free(output);
    *samples = mono;
    *frames = num_samples;
    *rate = sample_rate;
    return 1;
}

#if defined(__PS3__)
#include "ps3_paths.h"
extern "C" void write_log(const char *format, ...);
#endif

static int decode_path(const char *name, uae_s16 **samples, int *frames, int *rate)
{
    const char *prefixes[] = {
        "data/sounds",
        "./data/sounds",
        "/dev_hdd0/uae4all/sounds",
        "/dev_hdd0/uae4all/data/sounds",
        "./switchdata/data/sounds",
        NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", prefixes[i], name);
#if defined(__PS3__)
        char resolved[512];
        const char *p = ps3_resolve_path(path, resolved, sizeof(resolved));
        if (p && decode_ogg(p, samples, frames, rate)) {
            write_log("[PS3] disk_sound: loaded '%s' from '%s'\n", name, p);
            return 1;
        }
#else
        if (decode_ogg(path, samples, frames, rate))
            return 1;
#endif
    }
    return 0;
}
#endif

void disk_sound_init(void)
{
    disk_sound_shutdown();
    decode_path("floppy_drive.ogg", &floppy_samples, &floppy_frames, &floppy_rate);
    decode_path("hard_drive.ogg", &hard_samples, &hard_frames, &hard_rate);
}

void disk_sound_reset(void)
{
    pending_floppy_step = 0;
    pending_floppy_motor = 0;
    pending_floppy_read = 0;
    pending_floppy_write = 0;
    pending_hard_read = 0;
    pending_hard_write = 0;
    active_samples = NULL;
    active_frames = 0;
    active_rate = 0;
    active_position = 0;
    active_end = 0;
    active_step = 0;
    active_kind = 0;
    active_remaining = 0;
    active_length = 0;
    active_frequency = 0;
    active_volume = 0;
    active_phase = 0;
}

void disk_sound_set_volume(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    disk_sound_volume = percent;
}

void disk_sound_shutdown(void)
{
    disk_sound_reset();
    free(floppy_samples);
    free(hard_samples);
    floppy_samples = NULL;
    hard_samples = NULL;
    floppy_frames = 0;
    hard_frames = 0;
}

extern int mainMenu_bootHD;
extern char mainMenu_whdload_game[256];
extern char uae4all_image_file0[256];
extern char uae4all_image_file1[256];
extern char uae4all_image_file2[256];
extern char uae4all_image_file3[256];

static int is_floppy_active(void)
{
    if (cdrom_is_cd32_mode()) return 0;
    if (mainMenu_bootHD != 0) return 0;
    if (mainMenu_whdload_game[0] != '\0') return 0;
    if (uae4all_image_file0[0] != '\0' || uae4all_image_file1[0] != '\0' ||
        uae4all_image_file2[0] != '\0' || uae4all_image_file3[0] != '\0') {
        return 1;
    }
    return 0;
}

static void add_pending(volatile int *counter)
{
    if (*counter < 8) *counter = *counter + 1;
}

void disk_sound_floppy_step(void)
{
    if (!is_floppy_active()) return;
    add_pending(&pending_floppy_step);
}

void disk_sound_floppy_motor(void)
{
    if (!is_floppy_active()) return;
    add_pending(&pending_floppy_motor);
}

void disk_sound_floppy_read(void)
{
    if (!is_floppy_active()) return;
    add_pending(&pending_floppy_read);
}

void disk_sound_floppy_write(void)
{
    if (!is_floppy_active()) return;
    add_pending(&pending_floppy_write);
}

void disk_sound_hard_read(void)
{
    /* Floppy sound only for ADF disks, not hardfile/WHDLoad */
    return;
}

void disk_sound_hard_write(void)
{
    return;
}

static void set_sample(int event, int output_rate)
{
    int start_second;
    int length_milliseconds;
    uae_s16 *samples = event <= 4 ? floppy_samples : hard_samples;
    int frames = event <= 4 ? floppy_frames : hard_frames;
    int rate = event <= 4 ? floppy_rate : hard_rate;
    int start;
    int length;

    if (!samples || frames <= 0 || rate <= 0) return;
    start_second = event == 2 ? 0 : event == 1 ? 3 : event == 3 ? 3 : event == 4 ? 8 : 0;
    length_milliseconds = event == 2 ? 2800 : event == 1 ? 500 : event <= 4 ? 900 : event == 5 ? 800 : 1100;
    start = start_second * rate;
    length = (length_milliseconds * rate) / 1000;
    if (start >= frames) start = 0;
    if (length > frames - start) length = frames - start;
    if (length <= 0) return;
    active_samples = samples;
    active_frames = frames;
    active_rate = rate;
    active_position = ((uae_u64)start) << 32;
    active_end = ((uae_u64)(start + length)) << 32;
    active_step = ((uae_u64)rate << 32) / (uae_u32)output_rate;
    active_kind = 0;
}

static void start_next_sound(int output_rate)
{
    int event = 0;
    if (pending_floppy_step) {
        pending_floppy_step = pending_floppy_step - 1;
        event = 1;
        active_length = 1058;
        active_frequency = 820;
        active_volume = 950;
    } else if (pending_floppy_motor) {
        pending_floppy_motor = pending_floppy_motor - 1;
        event = 2;
        active_length = 2880;
        active_frequency = 125;
        active_volume = 500;
    } else if (pending_floppy_read) {
        pending_floppy_read = pending_floppy_read - 1;
        event = 3;
        active_length = 720;
        active_frequency = 420;
        active_volume = 300;
    } else if (pending_floppy_write) {
        pending_floppy_write = pending_floppy_write - 1;
        event = 4;
        active_length = 720;
        active_frequency = 310;
        active_volume = 340;
    } else if (pending_hard_read) {
        pending_hard_read = pending_hard_read - 1;
        event = 5;
        active_length = 480;
        active_frequency = 210;
        active_volume = 260;
    } else if (pending_hard_write) {
        pending_hard_write = pending_hard_write - 1;
        event = 6;
        active_length = 480;
        active_frequency = 165;
        active_volume = 280;
    } else {
        active_kind = 0;
        active_samples = NULL;
        active_remaining = 0;
        return;
    }
    active_samples = NULL;
    active_volume = active_volume * disk_sound_volume / 100;
    {
        static volatile int ss_log_count = 0;
        if (ss_log_count < 3) {
            ss_log_count = ss_log_count + 1;
            write_log ("[PS3] disk_sound: start event=%d frames=%d rate=%d out_rate=%d\n",
                       event, floppy_frames, floppy_rate, output_rate);
        }
    }
    set_sample(event, output_rate);
    if (!active_samples) {
        active_kind = event;
        active_remaining = active_length;
        active_phase = 0;
    }
}

static int triangle_sample(uae_u32 phase)
{
    int value = (int)((phase >> 16) & 0xffff);
    if (value > 32767) value = 65535 - value;
    return value * 2 - 32767;
}

void disk_sound_mix(uae_s16 *samples, int frames, int channels, int output_rate)
{
    int i;
    if (cdrom_is_cd32_mode()) return;
    if (!samples || frames <= 0 || channels <= 0 || output_rate <= 0) return;
    for (i = 0; i < frames; i++) {
        int value = 0;
        if (!active_samples && active_remaining <= 0 && !active_kind)
            start_next_sound(output_rate);
        if (active_samples) {
            int source_index = (int)(active_position >> 32);
            if (active_position >= active_end || source_index >= active_frames) {
                active_samples = NULL;
                continue;
            }
            value = active_samples[source_index] * active_volume / 1000;
            active_position += active_step;
        } else if (active_kind) {
            active_phase += (uae_u32)(((uae_u64)active_frequency << 32) / (uae_u32)output_rate);
            value = triangle_sample(active_phase);
            value = value * active_volume * active_remaining / (32767 * active_length);
            active_remaining--;
            if (active_remaining <= 0) active_kind = 0;
        } else {
            continue;
        }
        if (channels == 1) {
            int mixed = samples[i] + value;
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;
            samples[i] = (uae_s16)mixed;
        } else {
            int mixed_left = samples[i * channels] + value;
            int mixed_right = samples[i * channels + 1] + value;
            if (mixed_left > 32767) mixed_left = 32767;
            if (mixed_left < -32768) mixed_left = -32768;
            if (mixed_right > 32767) mixed_right = 32767;
            if (mixed_right < -32768) mixed_right = -32768;
            samples[i * channels] = (uae_s16)mixed_left;
            samples[i * channels + 1] = (uae_s16)mixed_right;
        }
    }
}
