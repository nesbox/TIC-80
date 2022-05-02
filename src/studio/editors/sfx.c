// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "sfx.h"
#include "ext/history.h"

#define DEFAULT_CHANNEL 0

enum 
{
    SFX_WAVE_PANEL,
    SFX_VOLUME_PANEL,
    SFX_CHORD_PANEL,
    SFX_PITCH_PANEL,
};

static tic_sample* getEffect(Sfx* sfx)
{
    return sfx->src->samples.data + sfx->index;
}

static tic_waveform* getWaveformById(Sfx* sfx, s32 i)
{
    return &sfx->src->waveforms.items[i];
}

static void drawPanelBorder(tic_mem* tic, s32 x, s32 y, s32 w, s32 h, tic_color color)
{
    tic_api_rect(tic, x, y, w, h, color);

    tic_api_rect(tic, x, y-1, w, 1, tic_color_dark_grey);
    tic_api_rect(tic, x-1, y, 1, h, tic_color_dark_grey);
    tic_api_rect(tic, x, y+h, w, 1, tic_color_light_grey);
    tic_api_rect(tic, x+w, y, 1, h, tic_color_light_grey);
}

static s32 hold(Sfx* sfx, s32 value)
{
    tic_mem* tic = sfx->tic;

    if(tic_api_key(tic, tic_key_ctrl) || 
        tic_api_key(tic, tic_key_shift))
    {
        if(sfx->holdValue < 0)
            sfx->holdValue = value;

        return sfx->holdValue;
    }

    return value;
}

static inline void unhold(Sfx* sfx)
{
    sfx->holdValue = -1;
}

static void drawCanvasLeds(Sfx* sfx, s32 x, s32 y, s32 canvasTab)
{
    tic_mem* tic = sfx->tic;

    enum 
    {
        Cols = SFX_TICKS, Rows = 16, 
        Gap = 1, LedWidth = 3 + Gap, LedHeight = 1 + Gap,
        Width = LedWidth * Cols + Gap, 
        Height = LedHeight * Rows + Gap
    };

    tic_api_rect(tic, x, y, Width, Height, tic_color_dark_grey);

    for(s32 i = 0; i < Height; i += LedHeight)
        tic_api_rect(tic, x, y + i, Width, Gap, tic_color_black);

    for(s32 i = 0; i < Width; i += LedWidth)
        tic_api_rect(tic, x + i, y, Gap, Height, tic_color_black);

    {
        const tic_sfx_pos* pos = &tic->ram->sfxpos[DEFAULT_CHANNEL];
        s32 tickIndex = *(pos->data + canvasTab);

        if(tickIndex >= 0)
            tic_api_rect(tic, x + tickIndex * LedWidth, y, LedWidth + 1, Height, tic_color_white);
    }

    tic_rect rect = {x, y, Width - Gap, Height - Gap};

    tic_sample* effect = getEffect(sfx);
    tic_rect border = {-1};

    if(checkMousePos(sfx->studio, &rect))
    {
        setCursor(sfx->studio, tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;
        mx /= LedWidth;
        s32 vy = my /= LedHeight;
        border = (tic_rect){x + mx * LedWidth + Gap, y + my * LedHeight + Gap, LedWidth - Gap, LedHeight - Gap};

        switch(canvasTab)
        {
        case SFX_VOLUME_PANEL: vy = MAX_VOLUME - my; break;
        case SFX_WAVE_PANEL:   sfx->hoverWave = vy = my = Rows - my - 1; break;
        case SFX_CHORD_PANEL:  vy = my = Rows - my - 1; break;
        case SFX_PITCH_PANEL:  vy = my = Rows / 2 - my - 1; break;
        default: break;
        }

        SHOW_TOOLTIP(sfx->studio, "[x=%02i y=%02i]", mx, vy);

        if(checkMouseDown(sfx->studio, &rect, tic_mouse_left))
        {
            my = hold(sfx, my);

            switch(canvasTab)
            {
            case SFX_WAVE_PANEL:   effect->data[mx].wave = my; break;
            case SFX_VOLUME_PANEL: effect->data[mx].volume = my; break;
            case SFX_CHORD_PANEL:  effect->data[mx].chord = my; break;
            case SFX_PITCH_PANEL:  effect->data[mx].pitch = my; break;
            default: break;
            }

            history_add(sfx->history);
        }
        else unhold(sfx);
    }

    for(s32 i = 0; i < Cols; i++)
    {
        switch(canvasTab)
        {
        case SFX_WAVE_PANEL:
            for(s32 j = 1, start = Height - LedHeight, value = effect->data[i].wave + 1; j <= value; j++, start -= LedHeight)
                tic_api_rect(tic, x + i * LedWidth + Gap, y + start, LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_red : tic_color_orange);
            break;

        case SFX_VOLUME_PANEL:
            for(s32 j = 1, start = Height - LedHeight, value = Rows - effect->data[i].volume; j <= value; j++, start -= LedHeight)
                tic_api_rect(tic, x + i * LedWidth + Gap, y + start, LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_blue : tic_color_light_blue);
            break;

        case SFX_CHORD_PANEL:
            for(s32 j = 1, start = Height - LedHeight, value = effect->data[i].chord + 1; j <= value; j++, start -= LedHeight)
                tic_api_rect(tic, x + i * LedWidth + Gap, y + start, LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_green : tic_color_light_green);
            break;

        case SFX_PITCH_PANEL:
            for(s32 value = effect->data[i].pitch, j = MIN(0, value); j <= MAX(0, value); j++)
                tic_api_rect(tic, x + i * LedWidth + Gap, y + (Height / 2 - (j + 1) * LedHeight + Gap),
                    LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_orange : tic_color_yellow);
            break;
        }
    }

    {
        tic_sound_loop* loop = effect->loops + canvasTab;
        if(loop->size > 0)
            for(s32 r = 0; r < Rows; r++)
            {
                tic_api_rect(tic, x + loop->start * LedWidth + 2, y + Gap + r * LedHeight, 1, 1, tic_color_white);
                tic_api_rect(tic, x + (loop->start + loop->size-1) * LedWidth + 2, y + Gap + r * LedHeight, 1, 1, tic_color_white);    
            }
    }

    if(border.x >= 0)
        tic_api_rectb(tic, border.x, border.y, border.w, border.h, tic_color_white);
}

static void drawVolumeStereo(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum {Width = TIC_ALTFONT_WIDTH-1, Height = TIC_FONT_HEIGHT};

    tic_sample* effect = getEffect(sfx);

    {
        tic_rect rect = {x, y, Width, Height};

        bool hover = false;
        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;

            showTooltip(sfx->studio, "left stereo");

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                effect->stereo_left = ~effect->stereo_left;
        }

        tic_api_print(tic, "L", rect.x, rect.y, effect->stereo_left ? hover ? tic_color_grey : tic_color_dark_grey : tic_color_light_green, true, 1, true);
    }

    {
        tic_rect rect = {x + 4, y, Width, Height};

        bool hover = false;
        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;

            showTooltip(sfx->studio, "right stereo");

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                effect->stereo_right = ~effect->stereo_right;
        }

        tic_api_print(tic, "R", rect.x, rect.y, effect->stereo_right ? hover ? tic_color_grey : tic_color_dark_grey : tic_color_light_green, true, 1, true);
    }
}

static void drawArppeggioSwitch(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    static const char Label[] = "DOWN";

    enum {Width = (sizeof Label - 1) * TIC_ALTFONT_WIDTH - 1, Height = TIC_FONT_HEIGHT};

    tic_sample* effect = getEffect(sfx);

    {
        tic_rect rect = {x, y, Width, Height};

        bool hover = false;
        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;

            showTooltip(sfx->studio, "up/down arpeggio");

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                effect->reverse = ~effect->reverse;
        }

        tic_api_print(tic, Label, rect.x, rect.y, effect->reverse ? tic_color_light_green : hover ? tic_color_grey : tic_color_dark_grey, true, 1, true);
    }
}

static void drawPitchSwitch(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    static const char Label[] = "x16";

    enum {Width = (sizeof Label - 1) * TIC_ALTFONT_WIDTH - 1, Height = TIC_FONT_HEIGHT};

    tic_sample* effect = getEffect(sfx);

    {
        tic_rect rect = {x, y, Width, Height};

        bool hover = false;
        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;

            showTooltip(sfx->studio, "x16 pitch");

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                effect->pitch16x = ~effect->pitch16x;
        }

        tic_api_print(tic, Label, rect.x, rect.y, effect->pitch16x ? tic_color_light_green : hover ? tic_color_grey : tic_color_dark_grey, true, 1, true);
    }
}

static void drawVolWaveSelector(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    typedef struct {const char* label; s32 panel; tic_rect rect; const char* tip;} Item;
    static const Item Items[] = 
    {
        {"WAV", SFX_WAVE_PANEL, {TIC_ALTFONT_WIDTH * 3, 0, TIC_ALTFONT_WIDTH * 3, TIC_FONT_HEIGHT}, "wave data"},
        {"VOL", SFX_VOLUME_PANEL, {0, 0, TIC_ALTFONT_WIDTH * 3, TIC_FONT_HEIGHT}, "volume data"},
    };

    for(s32 i = 0; i < COUNT_OF(Items); i++)
    {
        const Item* item = &Items[i];

        tic_rect rect = {x + item->rect.x, y + item->rect.y, item->rect.w, item->rect.h};

        bool hover = false;

        if(checkMousePos(sfx->studio, &rect))
        {
            showTooltip(sfx->studio, item->tip);

            setCursor(sfx->studio, tic_cursor_hand);

            hover = true;

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                sfx->volwave = item->panel;
        }

        tic_api_print(tic, item->label, x + item->rect.x, y + item->rect.y, item->panel == sfx->volwave ? tic_color_light_green : hover ? tic_color_grey : tic_color_dark_grey, true, 1, true);
    }
}

static void drawCanvas(Sfx* sfx, s32 x, s32 y, s32 canvasTab)
{
    tic_mem* tic = sfx->tic;

    enum 
    {
        Width = 147, Height = 33
    };

    drawPanelBorder(tic, x, y, Width, Height, tic_color_black);

    static const char* Labels[] = {"", "", "ARPEGG", "PITCH"};
    tic_api_print(tic, Labels[canvasTab], x + 2, y + 2, tic_color_dark_grey, true, 1, true);

    switch(canvasTab)
    {
    case SFX_WAVE_PANEL:
        drawVolWaveSelector(sfx, x + 2, y + 2);
        break;
    case SFX_VOLUME_PANEL:
        drawVolWaveSelector(sfx, x + 2, y + 2);
        drawVolumeStereo(sfx, x + 2, y + 9);
        break;
    case SFX_CHORD_PANEL:
        drawArppeggioSwitch(sfx, x + 2, y + 9);
        break;
    case SFX_PITCH_PANEL:
        drawPitchSwitch(sfx, x + 2, y + 9);
        break;
    default:
        break;
    }

    tic_api_print(tic, "LOOP:", x + 2, y + 20, tic_color_dark_grey, true, 1, true);

    enum
    {
        ArrowWidth = 3, ArrowHeight = 5
    };

    tic_sample* effect = getEffect(sfx);
    static const char SetLoopPosLabel[] = "set loop start";
    static const char SetLoopSizeLabel[] = "set loop size";

    {
        tic_rect rect = {x + 2, y + 27, ArrowWidth, ArrowHeight};
        bool hover = false;

        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;

            showTooltip(sfx->studio, SetLoopPosLabel);

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
            {
                effect->loops[canvasTab].start--;
                history_add(sfx->history);
            }
        }

        drawBitIcon(sfx->studio, tic_icon_left, rect.x - 2, rect.y - 1, hover ? tic_color_grey : tic_color_dark_grey);
    }

    {
        tic_rect rect = {x + 10, y + 27, ArrowWidth, ArrowHeight};
        bool hover = false;

        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;

            showTooltip(sfx->studio, SetLoopPosLabel);

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
            {
                effect->loops[canvasTab].start++;
                history_add(sfx->history);
            }
        }

        drawBitIcon(sfx->studio, tic_icon_right, rect.x - 2, rect.y - 1, hover ? tic_color_grey : tic_color_dark_grey);
    }

    {
        char buf[] = "0";
        sprintf(buf, "%X", effect->loops[canvasTab].start);
        tic_api_print(tic, buf, x + 6, y + 27, tic_color_grey, true, 1, true);
    }

    {
        tic_rect rect = {x + 14, y + 27, ArrowWidth, ArrowHeight};
        bool hover = false;

        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;
            showTooltip(sfx->studio, SetLoopSizeLabel);

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
            {
                effect->loops[canvasTab].size--;
                history_add(sfx->history);
            }
        }

        drawBitIcon(sfx->studio, tic_icon_left, rect.x - 2, rect.y - 1, hover ? tic_color_grey : tic_color_dark_grey);
    }

    {
        tic_rect rect = {x + 22, y + 27, ArrowWidth, ArrowHeight};
        bool hover = false;

        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);
            hover = true;
            showTooltip(sfx->studio, SetLoopSizeLabel);

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
            {
                effect->loops[canvasTab].size++;
                history_add(sfx->history);
            }
        }

        drawBitIcon(sfx->studio, tic_icon_right, rect.x - 2, rect.y - 1, hover ? tic_color_grey : tic_color_dark_grey);
    }

    {
        char buf[] = "0";
        sprintf(buf, "%X", effect->loops[canvasTab].size);
        tic_api_print(tic, buf, x + 18, y + 27, tic_color_grey, true, 1, true);
    }

    drawCanvasLeds(sfx, x + 26, y, canvasTab);
}

static void playSound(Sfx* sfx)
{
    if(sfx->play.active)
    {
        tic_sample* effect = getEffect(sfx);

        if(sfx->play.note != effect->note)
        {
            sfx->play.note = effect->note;

            sfx_stop(sfx->tic, DEFAULT_CHANNEL);
            tic_api_sfx(sfx->tic, sfx->index, effect->note, effect->octave, -1, DEFAULT_CHANNEL, MAX_VOLUME, MAX_VOLUME, SFX_DEF_SPEED);
        }
    }
    else
    {
        sfx->play.note = -1;
        sfx_stop(sfx->tic, DEFAULT_CHANNEL);
    }
}

static void undo(Sfx* sfx)
{
    history_undo(sfx->history);
}

static void redo(Sfx* sfx)
{
    history_redo(sfx->history);
}

static void copyToClipboard(Sfx* sfx)
{
    tic_sample* effect = getEffect(sfx);
    toClipboard(effect, sizeof(tic_sample), true);
}

static void resetSfx(Sfx* sfx)
{
    tic_sample* effect = getEffect(sfx);
    memset(effect, 0, sizeof(tic_sample));

    history_add(sfx->history);
}

static void cutToClipboard(Sfx* sfx)
{
    copyToClipboard(sfx);
    resetSfx(sfx);
}

static void copyFromClipboard(Sfx* sfx)
{
    tic_sample* effect = getEffect(sfx);

    if(fromClipboard(effect, sizeof(tic_sample), true, false, true))
        history_add(sfx->history);
}

static void processKeyboard(Sfx* sfx)
{
    tic_mem* tic = sfx->tic;

    if(tic->ram->input.keyboard.data == 0) return;

    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    s32 keyboardButton = -1;

    static const s32 Keycodes[] = 
    {
        tic_key_z,
        tic_key_s,
        tic_key_x,
        tic_key_d,
        tic_key_c,
        tic_key_v,
        tic_key_g,
        tic_key_b,
        tic_key_h,
        tic_key_n,
        tic_key_j,
        tic_key_m,
    };

    if(tic_api_key(tic, tic_key_alt))
        return;

    if(ctrl) {}
    else
    {
        for(int i = 0; i < COUNT_OF(Keycodes); i++)
            if(tic_api_key(tic, Keycodes[i]))
                keyboardButton = i;        
    }

    tic_sample* effect = getEffect(sfx);

    if(keyboardButton >= 0)
    {
        effect->note = keyboardButton;
        sfx->play.active = true;
    }

    if(tic_api_key(tic, tic_key_space))
        sfx->play.active = true;
}

static void processEnvelopesKeyboard(Sfx* sfx)
{
    tic_mem* tic = sfx->tic;
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    switch(getClipboardEvent(sfx->studio))
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(sfx); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(sfx); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(sfx); break;
    default: break;
    }

    if(ctrl)
    {
        if(keyWasPressed(sfx->studio, tic_key_z))        undo(sfx);
        else if(keyWasPressed(sfx->studio, tic_key_y))   redo(sfx);
    }

    else if(keyWasPressed(sfx->studio, tic_key_left))    sfx->index--;
    else if(keyWasPressed(sfx->studio, tic_key_right))   sfx->index++;
    else if(keyWasPressed(sfx->studio, tic_key_delete))  resetSfx(sfx);
}

static tic_waveform* getWave(Sfx* sfx)
{
    tic_sample* effect = getEffect(sfx);
    return getWaveformById(sfx, effect->data[0].wave);
}

static void copyWave(Sfx* sfx)
{
    toClipboard(getWave(sfx), sizeof(tic_waveform), true);
}

static void cutWave(Sfx* sfx)
{
    copyWave(sfx);

    memset(getWave(sfx), 0, sizeof(tic_waveform));
    history_add(sfx->waveHistory);
}

static void pasteWave(Sfx* sfx)
{
    if(fromClipboard(getWave(sfx), sizeof(tic_waveform), true, false, true))
        history_add(sfx->waveHistory);
}

static void undoWave(Sfx* sfx)
{
    history_undo(sfx->waveHistory);
}

static void redoWave(Sfx* sfx)
{
    history_redo(sfx->waveHistory);
}

static void drawWavesBar(Sfx* sfx, s32 x, s32 y)
{
    static struct Button 
    {
        u8 icon;
        const char* tip;
        void(*handler)(Sfx*);
    } Buttons[] = 
    {
        {
            tic_icon_cut,
            "CUT WAVE",
            cutWave,
        },
        {
            tic_icon_copy,
            "COPY WAVE",
            copyWave,
        },
        {
            tic_icon_paste,
            "PASTE WAVE",
            pasteWave,
        },
        {
            tic_icon_undo,
            "UNDO WAVE",
            undoWave,
        },
        {
            tic_icon_redo,
            "REDO WAVE",
            redoWave,
        },
    };

    enum {Size = 7};

    FOR(const struct Button*, it, Buttons)
    {
        tic_rect rect = {x, y, Size, Size};

        bool over = false;
        s32 push = 0;
        if(checkMousePos(sfx->studio, &rect))
        {
            over = true;
            setCursor(sfx->studio, tic_cursor_hand);

            showTooltip(sfx->studio, it->tip);

            if(checkMouseDown(sfx->studio, &rect, tic_mouse_left))
                push = 1;

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                it->handler(sfx);
        }

        if(over)
            drawBitIcon(sfx->studio, it->icon, rect.x, rect.y + 1, tic_color_black);

        drawBitIcon(sfx->studio, it->icon, rect.x, rect.y + push, 
            over ? tic_color_white : tic_color_dark_grey);

        y += Size;
    }
}

static void drawWaves(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum{Width = 10, Height = 6, MarginRight = 6, MarginBottom = 4, Cols = 4, Rows = 4, Scale = 4};

    for(s32 i = 0; i < WAVES_COUNT; i++)
    {
        s32 xi = i % Cols;
        s32 yi = i / Cols;

        tic_rect rect = {x + xi * (Width + MarginRight), y + yi * (Height + MarginBottom), Width, Height};
        tic_sample* effect = getEffect(sfx);
        bool hover = false;

        if(checkMousePos(sfx->studio, &rect))
        {
            setCursor(sfx->studio, tic_cursor_hand);

            hover = true;

            SHOW_TOOLTIP(sfx->studio, "select wave #%02i", i);

            if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
            {
                for(s32 c = 0; c < SFX_TICKS; c++)
                    effect->data[c].wave = i;

                history_add(sfx->history);
            }
        }

        bool sel = i == effect->data[0].wave;

        const tic_sfx_pos* pos = &tic->ram->sfxpos[DEFAULT_CHANNEL];
        bool active = *pos->data < 0 ? sfx->hoverWave == i : i == effect->data[*pos->data].wave;

        drawPanelBorder(tic, rect.x, rect.y, rect.w, rect.h, active ? tic_color_orange : sel ? tic_color_light_green : tic_color_black);

        // draw tiny wave previews
        {
            tic_waveform* wave = getWaveformById(sfx, i);

            for(s32 i = 0; i < WAVE_VALUES/Scale; i++)
            {
                s32 value = tic_tool_peek4(wave->data, i*Scale)/Scale;
                tic_api_pix(tic, rect.x + i+1, rect.y + Height - value - 2, 
                    active ? tic_color_red : sel ? tic_color_dark_green : hover ? tic_color_light_grey : tic_color_white, false);
            }

            // draw flare
            if(sel || active)
            {
                tic_api_rect(tic, rect.x + rect.w - 2, rect.y, 2, 1, tic_color_white);
                tic_api_pix(tic, rect.x + rect.w - 1, rect.y + 1, tic_color_white, false);
            }
        }
    }
}

static void drawWavePanel(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum {Width = 73, Height = 83, Round = 2};

    typedef struct {s32 x; s32 y; s32 x1; s32 y1; tic_color color;} Edge; 
    static const Edge Edges[] = 
    {
        {Width, Round, Width, Height - Round, tic_color_dark_grey},
        {Round, Height, Width - Round, Height, tic_color_dark_grey},
        {Width - Round, Height, Width, Height - Round, tic_color_dark_grey},
        {Width - Round, 0, Width, Round, tic_color_dark_grey},
        {0, Height - Round, Round, Height, tic_color_dark_grey},
        {Round, 0, Width - Round, 0, tic_color_light_grey},
        {0, Round, 0, Height - Round, tic_color_light_grey},
        {0, Round, Round, 0, tic_color_white},
    };

    FOR(const Edge*, edge, Edges)
        tic_api_line(tic, x + edge->x, y + edge->y, x + edge->x1, y + edge->y1, edge->color);

    // draw current wave shape
    {
        enum {Scale = 2, MaxValue = WAVE_MAX_VALUE};

        tic_rect rect = {x + 5, y + 5, 64, 32};
        tic_sample* effect = getEffect(sfx);
        tic_waveform* wave = getWaveformById(sfx, effect->data[0].wave);

        drawPanelBorder(tic, rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2, tic_color_light_green);

        if(sfx->play.active)
        {
            for(s32 i = 0; i < WAVE_VALUES; i++)
            {
                s32 amp = calcWaveAnimation(tic, i + sfx->play.tick, 0) / WAVE_MAX_VALUE;
                tic_api_rect(tic, rect.x + i*Scale, rect.y + (MaxValue - amp) * Scale, Scale, Scale, tic_color_dark_green);
            }
        }
        else
        {
            if(checkMousePos(sfx->studio, &rect))
            {
                setCursor(sfx->studio, tic_cursor_hand);

                s32 cx = (tic_api_mouse(tic).x - rect.x) / Scale;
                s32 cy = MaxValue - (tic_api_mouse(tic).y - rect.y) / Scale;

                SHOW_TOOLTIP(sfx->studio, "[x=%02i y=%02i]", cx, cy);

                enum {Border = 1};
                tic_api_rectb(tic, rect.x + cx*Scale - Border, 
                    rect.y + (MaxValue - cy) * Scale - Border, Scale + Border*2, Scale + Border*2, tic_color_dark_green);

                if(checkMouseDown(sfx->studio, &rect, tic_mouse_left))
                {
                    cy = hold(sfx, cy);

                    if(tic_tool_peek4(wave->data, cx) != cy)
                    {
                        tic_tool_poke4(wave->data, cx, cy);
                        history_add(sfx->waveHistory);
                    }
                }
                else unhold(sfx);
            }       

            for(s32 i = 0; i < WAVE_VALUES; i++)
            {
                s32 value = tic_tool_peek4(wave->data, i);
                tic_api_rect(tic, rect.x + i*Scale, rect.y + (MaxValue - value) * Scale, Scale, Scale, tic_color_dark_green);
            }
        }

        // draw flare
        {
            tic_api_rect(tic, rect.x + 59, rect.y + 2, 4, 1, tic_color_white);
            tic_api_rect(tic, rect.x + 62, rect.y + 2, 1, 3, tic_color_white);
        }
    }

    drawWaves(sfx, x + 5, y + 43);
    drawWavesBar(sfx, x + 65, y + 43);
}

static void drawPianoOctave(Sfx* sfx, s32 x, s32 y, s32 octave)
{
    tic_mem* tic = sfx->tic;

    enum 
    {
        Gap = 1, WhiteShadow = 1,
        WhiteWidth = 3, WhiteHeight = 8, WhiteCount = 7, WhiteWidthGap = WhiteWidth + Gap,
        BlackWidth = 3, BlackHeight = 4, BlackCount = 6, 
        BlackOffset = WhiteWidth - (BlackWidth - Gap) / 2,
        Width = WhiteCount * WhiteWidthGap - Gap,
        Height = WhiteHeight
    };

    tic_rect rect = {x, y, Width, Height};

    typedef struct{s32 note; tic_rect rect; bool white;} PianoBtn;
    static const PianoBtn Buttons[] = 
    {
        {0, WhiteWidthGap * 0, 0, WhiteWidth, WhiteHeight, true},
        {2, WhiteWidthGap * 1, 0, WhiteWidth, WhiteHeight, true},
        {4, WhiteWidthGap * 2, 0, WhiteWidth, WhiteHeight, true},
        {5, WhiteWidthGap * 3, 0, WhiteWidth, WhiteHeight, true},
        {7, WhiteWidthGap * 4, 0, WhiteWidth, WhiteHeight, true},
        {9, WhiteWidthGap * 5, 0, WhiteWidth, WhiteHeight, true},
        {11, WhiteWidthGap * 6, 0, WhiteWidth, WhiteHeight, true},

        {1, WhiteWidthGap * 0 + BlackOffset, 0, BlackWidth, BlackHeight, false},
        {3, WhiteWidthGap * 1 + BlackOffset, 0, BlackWidth, BlackHeight, false},
        {6, WhiteWidthGap * 3 + BlackOffset, 0, BlackWidth, BlackHeight, false},
        {8, WhiteWidthGap * 4 + BlackOffset, 0, BlackWidth, BlackHeight, false},
        {10, WhiteWidthGap * 5 + BlackOffset, 0, BlackWidth, BlackHeight, false},
    };

    tic_sample* effect = getEffect(sfx);

    s32 hover = -1;

    if(checkMousePos(sfx->studio, &rect))
    {
        for(s32 i = COUNT_OF(Buttons)-1; i >= 0; i--)
        {
            const PianoBtn* btn = Buttons + i;
            tic_rect btnRect = btn->rect;
            btnRect.x += x;
            btnRect.y += y;

            if(checkMousePos(sfx->studio, &btnRect))
            {
                setCursor(sfx->studio, tic_cursor_hand);

                hover = btn->note;

                {
                    static const char* Notes[] = SFX_NOTES;
                    SHOW_TOOLTIP(sfx->studio, "play %s%i note", Notes[btn->note], octave + 1);
                }

                if(checkMouseDown(sfx->studio, &rect, tic_mouse_left))
                {
                    effect->note = btn->note;
                    effect->octave = octave;
                    sfx->play.active = true;

                    history_add(sfx->history);
                }

                break;
            }
        }
    }

    bool active = sfx->play.active && effect->octave == octave;

    tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_dark_grey);

    for(s32 i = 0; i < COUNT_OF(Buttons); i++)
    {
        const PianoBtn* btn = Buttons + i;
        const tic_rect* rect = &btn->rect;
        tic_api_rect(tic, x + rect->x, y + rect->y, rect->w, rect->h, 
            active && effect->note == btn->note ? tic_color_red : 
                btn->white 
                    ? hover == btn->note ? tic_color_light_grey : tic_color_white 
                    : hover == btn->note ? tic_color_dark_grey : tic_color_black);

        if(btn->white)
            tic_api_rect(tic, x + rect->x, y + (WhiteHeight - WhiteShadow), WhiteWidth, WhiteShadow, tic_color_black);

        // draw current note marker
        if(effect->octave == octave && effect->note == btn->note)
            tic_api_rect(tic, x + rect->x + 1, y + rect->y + rect->h - 3, 1, 1, tic_color_red);
    }
}

static void drawPiano(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum {Width = 29};

    for(s32 i = 0; i < OCTAVES; i++)
    {
        drawPianoOctave(sfx, x + Width*i, y, i);
    }
}

static void drawSpeedPanel(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum 
    {
        Count = 8, Gap = 1, ColWidth = 1, ColWidthGap = ColWidth + Gap, 
        Width = Count * ColWidthGap - Gap, Height = 5,
        MaxSpeed = (1 << SFX_SPEED_BITS) / 2
    };

    tic_rect rect = {x + 13, y, Width, Height};
    tic_sample* effect = getEffect(sfx);
    s32 hover = -1;

    if(checkMousePos(sfx->studio, &rect))
    {
        setCursor(sfx->studio, tic_cursor_hand);

        s32 spd = (tic_api_mouse(tic).x - rect.x) / ColWidthGap;
        hover = spd;

        SHOW_TOOLTIP(sfx->studio, "set speed to %i", spd);

        if(checkMouseDown(sfx->studio, &rect, tic_mouse_left))
        {
            effect->speed = spd - MaxSpeed;
            history_add(sfx->history);
        }
    }

    tic_api_print(tic, "SPD", x, y, tic_color_dark_grey, true, 1, true);

    for(s32 i = 0; i < Count; i++)
        tic_api_rect(tic, rect.x + i * ColWidthGap, rect.y, ColWidth, rect.h, i - MaxSpeed <= effect->speed ? tic_color_light_green : hover == i ? tic_color_grey : tic_color_dark_grey);
}

static void drawSelectorPanel(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum
    {
        Size = 3, Gap = 1, SizeGap = Size + Gap, 
        GroupGap = 2, Groups = 4, Cols = 4, Rows = SFX_COUNT / (Cols * Groups),
        GroupWidth = Cols * SizeGap - Gap,
        Width = (GroupWidth + GroupGap) * Groups - GroupGap, Height = Rows * SizeGap - Gap
    };

    tic_rect rect = {x, y, Width, Height};
    s32 hover = -1;

    if(checkMousePos(sfx->studio, &rect))
        for(s32 g = 0, i = 0; g < Groups; g++)
            for(s32 r = 0; r < Rows; r++)
                for(s32 c = 0; c < Cols; c++, i++)
                {
                    tic_rect rect = {x + c * SizeGap + g * (GroupWidth + GroupGap), y + r * SizeGap, SizeGap, SizeGap};

                    if(checkMousePos(sfx->studio, &rect))
                    {
                        setCursor(sfx->studio, tic_cursor_hand);
                        hover = i;

                        SHOW_TOOLTIP(sfx->studio, "edit sfx #%02i", hover);

                        if(checkMouseClick(sfx->studio, &rect, tic_mouse_left))
                            sfx->index = i;

                        goto draw;
                    }
                }
draw:

    for(s32 g = 0, i = 0; g < Groups; g++)
        for(s32 r = 0; r < Rows; r++)
            for(s32 c = 0; c < Cols; c++, i++)
            {
                static const u8 EmptyEffect[sizeof(tic_sample)] = {0};
                bool empty = memcmp(sfx->src->samples.data + i, EmptyEffect, sizeof EmptyEffect) == 0;

                tic_api_rect(tic, x + c * SizeGap + g * (GroupWidth + GroupGap), y + r * SizeGap, Size, Size, 
                    sfx->index == i ? tic_color_light_green : hover == i ? tic_color_grey : empty ? tic_color_dark_grey : tic_color_light_grey);
            }
}

static void drawSelector(Sfx* sfx, s32 x, s32 y)
{
    tic_mem* tic = sfx->tic;

    enum {Width = 70, Height = 25};

    drawPanelBorder(tic, x, y, Width, Height, tic_color_black);

    {
        char buf[] = "00";
        sprintf(buf, "%02i", sfx->index);
        tic_api_print(tic, buf, x + 20, y + 2, tic_color_light_green, true, 1, true);
        tic_api_print(tic, "IDX", x + 6, y + 2, tic_color_dark_grey, true, 1, true);
    }

    drawSpeedPanel(sfx, x + 40, y + 2);
    drawSelectorPanel(sfx, x + 2, y + 9);
}

static void tick(Sfx* sfx)
{
    tic_mem* tic = sfx->tic;

    sfx->play.active = false;
    sfx->hoverWave = -1;

    processKeyboard(sfx);
    processEnvelopesKeyboard(sfx);

    tic_api_cls(tic, tic_color_grey);

    drawCanvas(sfx, 88, 12, sfx->volwave);
    drawCanvas(sfx, 88, 51, SFX_CHORD_PANEL);
    drawCanvas(sfx, 88, 90, SFX_PITCH_PANEL);

    drawSelector(sfx, 9, 12);
    drawPiano(sfx, 5, 127);
    drawWavePanel(sfx, 7, 41);
    drawToolbar(sfx->studio, tic, true);

    playSound(sfx);

    if(sfx->play.active)
        sfx->play.tick++;
    else 
        sfx->play.tick = 0;
}

static void onStudioEvent(Sfx* sfx, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT:   cutToClipboard(sfx); break;
    case TIC_TOOLBAR_COPY:  copyToClipboard(sfx); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(sfx); break;
    case TIC_TOOLBAR_UNDO:  undo(sfx); break;
    case TIC_TOOLBAR_REDO:  redo(sfx); break;
    default: break;
    }
}

void initSfx(Sfx* sfx, Studio* studio, tic_sfx* src)
{
    if(sfx->history) history_delete(sfx->history);
    if(sfx->waveHistory) history_delete(sfx->waveHistory);
    
    *sfx = (Sfx)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .tick = tick,
        .src = src,
        .index = 0,
        .volwave = SFX_VOLUME_PANEL,
        .hoverWave = -1,
        .holdValue = -1,
        .play = 
        {
            .note = -1,
            .active = false,
            .tick = 0,
        },

        .history = history_create(&src->samples, sizeof(tic_samples)),
        .waveHistory = history_create(&src->waveforms, sizeof(tic_waveforms)),
        .event = onStudioEvent,
    };
}

void freeSfx(Sfx* sfx)
{
    history_delete(sfx->history);
    history_delete(sfx->waveHistory);
    free(sfx);
}