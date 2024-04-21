// MIT License

// Copyright (c) 2020 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "api.h"
#include "core.h"

#include <string.h>
#include <limits.h>
#include "tic_assert.h"
#include "blip_buf.h"

#define ENVELOPE_FREQ_SCALE 2
#define SECONDS_PER_MINUTE 60
#define NOTES_PER_MINUTE (TIC80_FRAMERATE / NOTES_PER_BEAT * SECONDS_PER_MINUTE)
#define PIANO_START 8
#define ENDTIME (CLOCKRATE / TIC80_FRAMERATE)

static const u16 NoteFreqs[] = { 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1c, 0x1d, 0x1f, 0x21, 0x23, 0x25, 0x27, 0x29, 0x2c, 0x2e, 0x31, 0x34, 0x37, 0x3a, 0x3e, 0x41, 0x45, 0x49, 0x4e, 0x52, 0x57, 0x5c, 0x62, 0x68, 0x6e, 0x75, 0x7b, 0x83, 0x8b, 0x93, 0x9c, 0xa5, 0xaf, 0xb9, 0xc4, 0xd0, 0xdc, 0xe9, 0xf7, 0x106, 0x115, 0x126, 0x137, 0x14a, 0x15d, 0x172, 0x188, 0x19f, 0x1b8, 0x1d2, 0x1ee, 0x20b, 0x22a, 0x24b, 0x26e, 0x293, 0x2ba, 0x2e4, 0x310, 0x33f, 0x370, 0x3a4, 0x3dc, 0x417, 0x455, 0x497, 0x4dd, 0x527, 0x575, 0x5c8, 0x620, 0x67d, 0x6e0, 0x749, 0x7b8, 0x82d, 0x8a9, 0x92d, 0x9b9, 0xa4d, 0xaea, 0xb90, 0xc40, 0xcfa, 0xdc0, 0xe91, 0xf6f, 0x105a, 0x1153, 0x125b, 0x1372, 0x149a, 0x15d4, 0x1720, 0x1880 };
static_assert(COUNT_OF(NoteFreqs) == NOTES * OCTAVES + PIANO_START, "count_of_freqs");
static_assert(sizeof(tic_sound_register) == 16 + 2,                 "tic_sound_register");
static_assert(sizeof(tic_sample) == 66,                             "tic_sample");
static_assert(sizeof(tic_track_pattern) == 3 * MUSIC_PATTERN_ROWS,  "tic_track_pattern");
static_assert(sizeof(tic_track) == 3 * MUSIC_FRAMES + 3,            "tic_track");
static_assert(tic_music_cmd_count == 1 << MUSIC_CMD_BITS,           "tic_music_cmd_count");
static_assert(sizeof(tic_music_state) == 4,                         "tic_music_state_size");

static s32 getTempo(tic_core* core, const tic_track* track)
{
    return core->state.music.tempo < 0
        ? track->tempo + DEFAULT_TEMPO
        : core->state.music.tempo;
}

static s32 getSpeed(tic_core* core, const tic_track* track)
{
    return core->state.music.speed < 0
        ? track->speed + DEFAULT_SPEED
        : core->state.music.speed;
}

static s32 tick2row(tic_core* core, const tic_track* track, s32 tick)
{
    // BPM = tempo * 6 / speed
    s32 speed = getSpeed(core, track);
    return speed
        ? tick * getTempo(core, track) * DEFAULT_SPEED / speed / NOTES_PER_MINUTE
        : 0;
}

static s32 row2tick(tic_core* core, const tic_track* track, s32 row)
{
    s32 tempo = getTempo(core, track);
    return tempo
        ? row * getSpeed(core, track) * NOTES_PER_MINUTE / tempo / DEFAULT_SPEED
        : 0;
}

static inline s32 param2val(const tic_track_row* row)
{
    return (row->param1 << 4) | row->param2;
}

static void update_amp(blip_buffer_t* blip, tic_sound_register_data* data, s32 new_amp)
{
    s32 delta = new_amp - data->amp;
    data->amp += delta;
    blip_add_delta(blip, data->time, delta);
}

static inline s32 freq2period(s32 freq)
{
    enum
    {
        MinPeriodValue = 10,
        MaxPeriodValue = 4096,
        Rate = CLOCKRATE * ENVELOPE_FREQ_SCALE / WAVE_VALUES
    };

    if (freq == 0) return MaxPeriodValue;

    return CLAMP(Rate / freq - 1, MinPeriodValue, MaxPeriodValue);
}

static inline s32 getAmp(s32 volume, s32 amp)
{
    return amp * volume / MAX_VOLUME / (TIC_SOUND_CHANNELS + 1);
}

static void runPcm(blip_buffer_t* blip, const tic_pcm* pcm, tic_sound_register_data* data)
{
    enum{Period = ENDTIME / TIC_PCM_SIZE};

    for (data->time = 0; data->time < ENDTIME; data->time += Period, data->phase = (data->phase + 1) % TIC_PCM_SIZE)
    {
        update_amp(blip, data, getAmp(MAX_VOLUME, pcm->data[data->phase] * SHRT_MAX / UCHAR_MAX));
    }
}

static void runEnvelope(blip_buffer_t* blip, const tic_sound_register* reg, tic_sound_register_data* data, u8 stereo_volume)
{
    s32 period = freq2period(tic_sound_register_get_freq(reg) * ENVELOPE_FREQ_SCALE);

    for (; data->time < ENDTIME; data->time += period, data->phase = (data->phase + 1) % WAVE_VALUES)
    {
        update_amp(blip, data, getAmp(reg->volume, tic_tool_peek4(reg->waveform.data, data->phase) * SHRT_MAX / MAX_VOLUME * stereo_volume / MAX_VOLUME));
    }
}

static void runNoise(blip_buffer_t* blip, const tic_sound_register* reg, tic_sound_register_data* data, u8 stereo_volume)
{
    // phase is noise LFSR, which must never be zero
    if (data->phase == 0)
        data->phase = 1;

    s32 period = freq2period(tic_sound_register_get_freq(reg));
    s32 fb = *reg->waveform.data ? 0x14 : 0x12000;

    for (; data->time < ENDTIME; data->time += period, data->phase = ((data->phase & 1) * fb) ^ (data->phase >> 1))
    {
        update_amp(blip, data, getAmp(reg->volume, (data->phase & 1) ? stereo_volume * SHRT_MAX / MAX_VOLUME : 0));
    }
}

static s32 calcLoopPos(const tic_sound_loop* loop, s32 pos)
{
    s32 offset = 0;

    if (loop->size > 0)
    {
        for (s32 i = 0; i < pos; i++)
        {
            if (offset < (loop->start + loop->size - 1))
                offset++;
            else offset = loop->start;
        }
    }
    else offset = pos >= SFX_TICKS ? SFX_TICKS - 1 : pos;

    return offset;
}

static void resetSfxPos(tic_channel_data* channel)
{
    memset(channel->pos->data, -1, sizeof(tic_sfx_pos));
    channel->tick = -1;
}

static void sfx(tic_mem* memory, s32 index, s32 note, s32 pitch, tic_channel_data* channel, tic_sound_register* reg, s32 channelIndex)
{
    tic_core* core = (tic_core*)memory;

    if (channel->duration > 0)
        channel->duration--;

    if (index < 0 || channel->duration == 0)
    {
        resetSfxPos(channel);
        return;
    }

    const tic_sample* effect = &memory->ram->sfx.samples.data[index];
    s32 pos = tic_tool_sfx_pos(channel->speed, ++channel->tick);

    for (s32 i = 0; i < sizeof(tic_sfx_pos); i++)
        *(channel->pos->data + i) = calcLoopPos(effect->loops + i, pos);

    u8 volume = MAX_VOLUME - effect->data[channel->pos->volume].volume;

    if (volume > 0)
    {
        s8 arp = effect->data[channel->pos->chord].chord * (effect->reverse ? -1 : 1);
        if (arp) note += arp;

        note = CLAMP(note, 0, COUNT_OF(NoteFreqs) - 1);

        tic_sound_register_set_freq(reg, NoteFreqs[note] + effect->data[channel->pos->pitch].pitch * (effect->pitch16x ? 16 : 1) + pitch);
        reg->volume = volume;

        u8 wave = effect->data[channel->pos->wave].wave;
        const tic_waveform* waveform = &memory->ram->sfx.waveforms.items[wave];
        memcpy(reg->waveform.data, waveform->data, sizeof(tic_waveform));

        tic_tool_poke4(&memory->ram->stereo.data, channelIndex * 2, channel->volume.left * !effect->stereo_left);
        tic_tool_poke4(&memory->ram->stereo.data, channelIndex * 2 + 1, channel->volume.right * !effect->stereo_right);
    }
}

static void setChannelData(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, tic_channel_data* channel, s32 volumeLeft, s32 volumeRight, s32 speed)
{
    tic_core* core = (tic_core*)memory;

    channel->volume.left = volumeLeft;
    channel->volume.right = volumeRight;

    if (index >= 0)
    {
        struct { s8 speed : SFX_SPEED_BITS; } temp = { speed };
        channel->speed = speed == temp.speed ? speed : memory->ram->sfx.samples.data[index].speed;
    }

    channel->note = note + octave * NOTES;
    channel->duration = duration;
    channel->index = index;

    resetSfxPos(channel);
}


static void setMusicChannelData(tic_mem* memory, s32 index, s32 note, s32 octave, s32 left, s32 right, s32 channel)
{
    tic_core* core = (tic_core*)memory;
    setChannelData(memory, index, note, octave, -1, &core->state.music.channels[channel], left, right, SFX_DEF_SPEED);
}

static void resetMusicChannels(tic_mem* memory)
{
    for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
        setMusicChannelData(memory, -1, 0, 0, 0, 0, c);

    tic_core* core = (tic_core*)memory;
    memset(core->state.music.commands, 0, sizeof core->state.music.commands);
    memset(&core->state.music.jump, 0, sizeof(tic_jump_command));
}

static void stopMusic(tic_mem* memory)
{
    tic_api_music(memory, -1, 0, 0, false, false, -1, -1);
}

static void processMusic(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    tic_music_state* music_state = &memory->ram->music_state;

    if (music_state->flag.music_status == tic_music_stop) return;

    const tic_track* track = &memory->ram->music.tracks.data[music_state->music.track];
    s32 row = tick2row(core, track, core->state.music.ticks);
    tic_jump_command* jumpCmd = &core->state.music.jump;

    if (row != music_state->music.row
        && jumpCmd->active)
    {
        music_state->music.frame = jumpCmd->frame;
        row = jumpCmd->beat * NOTES_PER_BEAT;
        core->state.music.ticks = row2tick(core, track, row);
        memset(jumpCmd, 0, sizeof(tic_jump_command));
    }

    s32 rows = MUSIC_PATTERN_ROWS - track->rows;
    if (row >= rows)
    {
        row = 0;
        core->state.music.ticks = 0;

        // If music is in sustain mode, we only reset the channels if the music stopped.
        // Otherwise, we reset it on every new frame.
        if (music_state->flag.music_status == tic_music_stop || !music_state->flag.music_sustain)
        {
            resetMusicChannels(memory);

            for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
                setMusicChannelData(memory, -1, 0, 0, MAX_VOLUME, MAX_VOLUME, c);
        }

        if (music_state->flag.music_status == tic_music_play)
        {
            music_state->music.frame++;

            if (music_state->music.frame >= MUSIC_FRAMES)
            {
                if (music_state->flag.music_loop)
                    music_state->music.frame = 0;
                else
                {
                    stopMusic(memory);
                    return;
                }
            }
            else
            {
                s32 val = 0;
                for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
                    val += tic_tool_get_pattern_id(track, music_state->music.frame, c);

                // empty frame detected
                if (!val)
                {
                    if (music_state->flag.music_loop)
                        music_state->music.frame = 0;
                    else
                    {
                        stopMusic(memory);
                        return;
                    }
                }
            }
        }
        else if (music_state->flag.music_status == tic_music_play_frame)
        {
            if (!music_state->flag.music_loop)
            {
                stopMusic(memory);
                return;
            }
        }
    }

    if (row != music_state->music.row)
    {
        music_state->music.row = row;

        for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
        {
            s32 patternId = tic_tool_get_pattern_id(track, music_state->music.frame, c);
            if (!patternId) continue;

            const tic_track_pattern* pattern = &memory->ram->music.patterns.data[patternId - PATTERN_START];
            const tic_track_row* trackRow = &pattern->rows[music_state->music.row];
            tic_channel_data* channel = &core->state.music.channels[c];
            tic_command_data* cmdData = &core->state.music.commands[c];

            if (trackRow->command == tic_music_cmd_delay)
            {
                cmdData->delay.row = trackRow;
                cmdData->delay.ticks = param2val(trackRow);
                trackRow = NULL;
            }

            if (cmdData->delay.row && cmdData->delay.ticks == 0)
            {
                trackRow = cmdData->delay.row;
                cmdData->delay.row = NULL;
            }

            if (trackRow)
            {
                // reset commands data
                if (trackRow->note)
                {
                    cmdData->slide.tick = 0;
                    cmdData->slide.note = channel->note;
                }

                if (trackRow->note == NoteStop)
                    setMusicChannelData(memory, -1, 0, 0, channel->volume.left, channel->volume.right, c);
                else if (trackRow->note >= NoteStart)
                    setMusicChannelData(memory, tic_tool_get_track_row_sfx(trackRow), trackRow->note - NoteStart, trackRow->octave,
                        channel->volume.left, channel->volume.right, c);

                switch (trackRow->command)
                {
                case tic_music_cmd_volume:
                    channel->volume.left = trackRow->param1;
                    channel->volume.right = trackRow->param2;
                    break;

                case tic_music_cmd_chord:
                    cmdData->chord.tick = 0;
                    cmdData->chord.note1 = trackRow->param1;
                    cmdData->chord.note2 = trackRow->param2;
                    break;

                case tic_music_cmd_jump:
                    core->state.music.jump.active = true;
                    core->state.music.jump.frame = trackRow->param1;
                    core->state.music.jump.beat = trackRow->param2;
                    break;

                case tic_music_cmd_vibrato:
                    cmdData->vibrato.tick = 0;
                    cmdData->vibrato.period = trackRow->param1;
                    cmdData->vibrato.depth = trackRow->param2;
                    break;

                case tic_music_cmd_slide:
                    cmdData->slide.duration = param2val(trackRow);
                    break;

                case tic_music_cmd_pitch:
                    cmdData->finepitch.value = param2val(trackRow) - PITCH_DELTA;
                    break;

                default: break;
                }
            }
        }
    }

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i)
    {
        tic_channel_data* channel = &core->state.music.channels[i];
        tic_command_data* cmdData = &core->state.music.commands[i];

        if (channel->index >= 0)
        {
            s32 note = channel->note;
            s32 pitch = 0;

            // process chord command
            {
                s32 chord[] =
                {
                    0,
                    cmdData->chord.note1,
                    cmdData->chord.note2
                };

                note += chord[cmdData->chord.tick % (cmdData->chord.note2 == 0 ? 2 : 3)];
            }

            // process vibrato command
            if (cmdData->vibrato.period && cmdData->vibrato.depth)
            {
                static const s32 VibData[] = { 0x0, 0x31f1, 0x61f8, 0x8e3a, 0xb505, 0xd4db, 0xec83, 0xfb15, 0x10000, 0xfb15, 0xec83, 0xd4db, 0xb505, 0x8e3a, 0x61f8, 0x31f1, 0x0, 0xffffce0f, 0xffff9e08, 0xffff71c6, 0xffff4afb, 0xffff2b25, 0xffff137d, 0xffff04eb, 0xffff0000, 0xffff04eb, 0xffff137d, 0xffff2b25, 0xffff4afb, 0xffff71c6, 0xffff9e08, 0xffffce0f };
                static_assert(COUNT_OF(VibData) == 32, "VibData");

                s32 p = cmdData->vibrato.period << 1;
                pitch += (VibData[(cmdData->vibrato.tick % p) * COUNT_OF(VibData) / p] * cmdData->vibrato.depth) >> 16;
            }

            // process slide command
            if (cmdData->slide.tick < cmdData->slide.duration)
                pitch += (NoteFreqs[channel->note] - NoteFreqs[note = cmdData->slide.note]) * cmdData->slide.tick / cmdData->slide.duration;

            pitch += cmdData->finepitch.value;

            sfx(memory, channel->index, note, pitch, channel, &memory->ram->registers[i], i);
        }

        ++cmdData->chord.tick;
        ++cmdData->vibrato.tick;
        ++cmdData->slide.tick;

        if (cmdData->delay.ticks)
            cmdData->delay.ticks--;
    }

    core->state.music.ticks++;
}

static void setSfxChannelData(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 left, s32 right, s32 speed)
{
    tic_core* core = (tic_core*)memory;
    setChannelData(memory, index, note, octave, duration, &core->state.sfx.channels[channel], left, right, speed);
}

static void setMusic(tic_core* core, s32 index, s32 frame, s32 row, bool loop, bool sustain, s32 tempo, s32 speed)
{
    tic_mem* memory = (tic_mem*)core;
    tic_ram* ram = memory->ram;

    ram->music_state.music.track = index;

    if (index < 0)
    {
        ram->music_state.flag.music_status = tic_music_stop;
        resetMusicChannels(memory);
    }
    else
    {
        for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
            setMusicChannelData(memory, -1, 0, 0, MAX_VOLUME, MAX_VOLUME, c);

        ram->music_state.music.row = -1;
        ram->music_state.music.frame = frame < 0 ? 0 : frame;
        ram->music_state.flag.music_loop = loop;
        ram->music_state.flag.music_sustain = sustain;
        ram->music_state.flag.music_status = tic_music_play;

        const tic_track* track = &ram->music.tracks.data[index];
        core->state.music.tempo = tempo;
        core->state.music.speed = speed;
        core->state.music.ticks = row >= 0 ? row2tick(core, track, row) : 0;
    }
}

void tic_api_music(tic_mem* memory, s32 index, s32 frame, s32 row, bool loop, bool sustain, s32 tempo, s32 speed)
{
    tic_core* core = (tic_core*)memory;

    setMusic(core, index, frame, row, loop, sustain, tempo, speed);

    if (index >= 0)
        memory->ram->music_state.flag.music_status = tic_music_play;
}

void tic_api_sfx(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 left, s32 right, s32 speed)
{
    tic_core* core = (tic_core*)memory;
    setSfxChannelData(memory, index, note, octave, duration, channel, left, right, speed);
}

static inline const struct sound_ring_buf *sound_ringbuf(tic_core* core)
{
    return &core->state.sound_ringbuf[(core->state.sound_ringbuf_tail + TIC_SOUND_RINGBUF_LEN - 1) % TIC_SOUND_RINGBUF_LEN];
}

static void stereo_synthesize(tic_core* core, struct sound_register_data *regdata, blip_buffer_t* blip, u8 stereoRight)
{
    const struct sound_ring_buf *ringbuf = sound_ringbuf(core);

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i)
    {
        u8 stereo_volume = tic_tool_peek4(&ringbuf->stereo, stereoRight + i * 2);

        const tic_sound_register* reg = &ringbuf->registers[i];
        tic_sound_register_data* data = &regdata->data[i];

        tic_tool_noise(&reg->waveform)
            ? runNoise(blip, reg, data, stereo_volume)
            : runEnvelope(blip, reg, data, stereo_volume);

        data->time -= ENDTIME;
    }

    runPcm(blip, &ringbuf->pcm, &regdata->pcm);

    blip_end_frame(blip, ENDTIME);
}

void tic_core_synth_sound(tic_mem* memory)
{
    tic_core *core = (tic_core*)memory;
    tic80 *product = &core->memory.product;

    // synthesize sound using the register values found from the tail of the ring buffer
    stereo_synthesize(core, &core->state.registers.left, core->blip.left, 0);
    stereo_synthesize(core, &core->state.registers.right, core->blip.right, 1);

    blip_read_samples(core->blip.left, product->samples.buffer, core->samplerate / TIC80_FRAMERATE, TIC80_SAMPLE_CHANNELS);
    blip_read_samples(core->blip.right, product->samples.buffer + 1, core->samplerate / TIC80_FRAMERATE, TIC80_SAMPLE_CHANNELS);

    // if the head has advanced, we can advance the tail too. Otherwise, we just
    // keep synthesizing audio using the last known register values, so at least we don't get crackles
    if (core->state.sound_ringbuf_tail != core->state.sound_ringbuf_head) {
        // note: we assume storing a 32 bit integer is atomic, that should hold on pretty much any modern processor
        // assuming it is aligned in memory (which it should be)
        core->state.sound_ringbuf_tail = (core->state.sound_ringbuf_tail + 1) % TIC_SOUND_RINGBUF_LEN;
    }
}

void tic_core_sound_tick_start(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    ZEROMEM(memory->ram->registers);
    ZEROMEM(memory->ram->pcm);

    memory->ram->stereo.data = -1;

    processMusic(memory);

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i)
    {
        tic_channel_data* c = &core->state.sfx.channels[i];

        if (c->index >= 0)
            sfx(memory, c->index, c->note, 0, c, &memory->ram->registers[i], i);
    }
}

void tic_core_sound_tick_end(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    // instead of synthesizing the sound right away, push the sound registers to the head of a ring buffer
    struct sound_ring_buf *ringbuf = &core->state.sound_ringbuf[core->state.sound_ringbuf_head];
    memcpy(&ringbuf->registers, &memory->ram->registers, sizeof ringbuf->registers);
    ringbuf->stereo = memory->ram->stereo;
    ringbuf->pcm = memory->ram->pcm;

    if (core->state.sound_ringbuf_head != (core->state.sound_ringbuf_tail + TIC_SOUND_RINGBUF_LEN - 2) % TIC_SOUND_RINGBUF_LEN) {
        // note: we assume storing a 32 bit integer is atomic, that should hold on pretty much any modern processor
        // assuming it is aligned in memory (which it should be)
        core->state.sound_ringbuf_head = (core->state.sound_ringbuf_head + 1) % TIC_SOUND_RINGBUF_LEN;
    }
}
