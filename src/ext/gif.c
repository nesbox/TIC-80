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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gif.h"
#include "gif_lib.h"

static gif_image* readGif(GifFileType *gif)
{
    gif_image* image = NULL;

    s32 error = 0;

    if(gif)
    {
        if(gif->SHeight > 0 && gif->SWidth > 0)
        {
            s32 size = gif->SWidth * gif->SHeight * sizeof(GifPixelType);
            GifPixelType* screen = (GifPixelType*)malloc(size);

            if(screen)
            {
                memset(screen, gif->SBackGroundColor, size);

                GifRecordType record = UNDEFINED_RECORD_TYPE;

                do
                {
                    if(DGifGetRecordType(gif, &record) == GIF_ERROR)
                    {
                        error = gif->Error;
                        break;
                    }

                    switch (record)
                    {
                    case IMAGE_DESC_RECORD_TYPE:
                        {
                            if(DGifGetImageDesc(gif) == GIF_ERROR)
                                error = gif->Error;

                            s32 row = gif->Image.Top;
                            s32 col = gif->Image.Left;
                            s32 width = gif->Image.Width;
                            s32 height = gif->Image.Height;

                            if (gif->Image.Left + gif->Image.Width > gif->SWidth ||
                                gif->Image.Top + gif->Image.Height > gif->SHeight)
                                    error = E_GIF_ERR_OPEN_FAILED;

                            if (gif->Image.Interlace)
                            {
                                s32 InterlacedOffset[] = { 0, 4, 2, 1 };
                                s32 InterlacedJumps[] = { 8, 8, 4, 2 };

                                for (s32 i = 0; i < 4; i++)
                                    for (s32 j = row + InterlacedOffset[i]; j < row + height; j += InterlacedJumps[i])
                                    {
                                        if(DGifGetLine(gif, screen + j * gif->SWidth + col, width) == GIF_ERROR)
                                        {
                                            error = gif->Error;
                                            break;
                                        }
                                    }
                            }
                            else
                            {
                                for (s32 i = 0; i < height; i++, row++)
                                {
                                    if(DGifGetLine(gif, screen + row * gif->SWidth + col, width) == GIF_ERROR)
                                    {
                                        error = gif->Error;
                                        break;
                                    }
                                }
                            }
                        }
                        break;

                    case EXTENSION_RECORD_TYPE:
                        {
                            s32 extCode = 0;
                            GifByteType* extension = NULL;

                            if (DGifGetExtension(gif, &extCode, &extension) == GIF_ERROR)
                                error = gif->Error;
                            else
                            {
                                while (extension != NULL)
                                {
                                    if(DGifGetExtensionNext(gif, &extension) == GIF_ERROR)
                                    {
                                        error = gif->Error;
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    case TERMINATE_RECORD_TYPE:
                        break;
                    default: break;
                    }

                    if(error != E_GIF_SUCCEEDED)
                        break;
                }
                while(record != TERMINATE_RECORD_TYPE);

                if(error == E_GIF_SUCCEEDED)
                {

                    image = (gif_image*)malloc(sizeof(gif_image));

                    if(image)
                    {
                        memset(image, 0, sizeof(gif_image));
                        image->buffer = screen;
                        image->width = gif->SWidth;
                        image->height = gif->SHeight;

                        ColorMapObject* colorMap = gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap;

                        image->colors = colorMap->ColorCount;

                        s32 size = image->colors * sizeof(gif_color);
                        image->palette = malloc(size);

                        memcpy(image->palette, colorMap->Colors, size);
                    }                   
                }
                else free(screen);
            }
        }

        DGifCloseFile(gif, &error);
    }

    return image;
}

typedef struct
{
    const void* data;
    s32 pos;
} GifBuffer;

static s32 readBuffer(GifFileType* gif, GifByteType* data, s32 size)
{
    GifBuffer* buffer = (GifBuffer*)gif->UserData;

    memcpy(data, (const u8*)buffer->data + buffer->pos, size);
    buffer->pos += size;

    return size;
}

gif_image* gif_read_data(const void* data, s32 size)
{
    GifBuffer buffer = {data, 0};
    GifFileType *gif = DGifOpen(&buffer, readBuffer, NULL);

    return readGif(gif);
}

void gif_close(gif_image* image)
{
    if(image)
    {
        if(image->buffer) free(image->buffer);
        if(image->palette) free(image->palette);

        free(image);
    }
}
