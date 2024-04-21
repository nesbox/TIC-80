#include "tic.h"
#include <string.h>

bool parse_note(const char* noteStr, s32* note, s32* octave)
{
    if(noteStr && strlen(noteStr) == 3)
    {
        static const char* Notes[] = SFX_NOTES;

        for(s32 i = 0; i < COUNT_OF(Notes); i++)
        {
            if(memcmp(Notes[i], noteStr, 2) == 0)
            {
                *note = i;
                *octave = noteStr[2] - '1';
                break;
            }
        }

        return true;
    }

    return false;
}
