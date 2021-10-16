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

static bool writeGif(GifFileType* gif, s32 width, s32 height, const u8* data, const gif_color* palette, u8 bpp)
{
	bool result = false;
	s32 error = 0;

	if(gif)
	{
		s32 colors = 1 << bpp;
		ColorMapObject* colorMap = GifMakeMapObject(colors, NULL);

		memcpy(colorMap->Colors, palette, colors * sizeof(GifColorType));

		if(EGifPutScreenDesc(gif, width, height, bpp, 0, colorMap) != GIF_ERROR)
		{
			if(EGifPutImageDesc(gif, 0, 0, width, height, false, NULL) != GIF_ERROR)
			{
				GifByteType* ptr = (GifByteType*)data;
				for (s32 i = 0; i < height; i++, ptr += width)
				{
					if (EGifPutLine(gif, ptr, width) == GIF_ERROR)
					{
						error = gif->Error;
						break;
					}
				}

				result = error == E_GIF_SUCCEEDED;
			}
		}

		EGifCloseFile(gif, &error);
		GifFreeMapObject(colorMap);
	}

	return result;
}

static s32 writeBuffer(GifFileType* gif, const GifByteType* data, s32 size)
{
	GifBuffer* buffer = (GifBuffer*)gif->UserData;

	memcpy((u8*)buffer->data + buffer->pos, data, size);
	buffer->pos += size;

	return size;
}

bool gif_write_data(const void* buffer, s32* size, s32 width, s32 height, const u8* data, const gif_color* palette, u8 bpp)
{
	s32 error = 0;
	GifBuffer output = {buffer, 0};
	GifFileType* gif = EGifOpen(&output, writeBuffer, &error);

	bool result = writeGif(gif, width, height, data, palette, bpp);

	*size = output.pos;

	return result;
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

static bool AddLoop(GifFileType *gif)
{
	{
		const char *nsle = "NETSCAPE2.0";
		const char subblock[] = {
			1, // always 1
			0, // little-endian loop counter:
			0  // 0 for infinite loop.
		};

		EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE);
		EGifPutExtensionBlock(gif, 11, nsle);
		EGifPutExtensionBlock(gif, 3, subblock);
		EGifPutExtensionTrailer(gif);
	}

	return true;
}

static const u8* toColor(const u8* ptr, gif_color* color)
{
	color->r = *ptr++;
	color->g = *ptr++;
	color->b = *ptr++;
	ptr++;

	return ptr;
}

bool gif_write_animation(const void* buffer, s32* size, s32 width, s32 height, const u8* data, s32 frames, s32 fps, s32 scale)
{
	bool result = false;

	s32 swidth = width*scale, sheight = height*scale;
	s32 frameSize = width * height;

	enum{Bpp = 8, PalSize = 1 << Bpp, PalStructSize = PalSize * sizeof(gif_color)};

	s32 error = 0;
	GifBuffer output = {buffer, 0};
	GifFileType* gif = EGifOpen(&output, writeBuffer, &error);

	if(gif)
	{			
		EGifSetGifVersion(gif, true);

		if(EGifPutScreenDesc(gif, swidth, sheight, Bpp, 0, NULL) != GIF_ERROR)
		{
			if(AddLoop(gif))
			{
				gif_color* palette = (gif_color*)malloc(PalStructSize);					
				u8* screen = malloc(frameSize);
				u8* line = malloc(swidth);

				for(s32 f = 0; f < frames; f++)
				{
					enum {DelayUnits = 100, MinDelay = 2};

					s32 frame = (f * fps * MinDelay * 2 + 1) / (2 * DelayUnits);

					if(frame >= frames)
						break;

					s32 colors = 0;
					const u8* ptr = data + frameSize*frame*sizeof(u32);
					
					{
						memset(palette, 0, PalStructSize);
						memset(screen, 0, frameSize);

						for(s32 i = 0; i < frameSize; i++)
						{
							if(colors >= PalSize) break;

							gif_color color;
							toColor(ptr + i*sizeof(u32), &color);

							bool found = false;
							for(s32 c = 0; c < colors; c++)
							{
								if(memcmp(&palette[c], &color, sizeof(gif_color)) == 0)
								{
									found = true;
									screen[i] = c;
									break;
								}
							}

							if(!found)
							{
								// TODO: check for last color in palette and try to find closest color
								screen[i] = colors;
								memcpy(&palette[colors], &color, sizeof(gif_color));
								colors++;
							}
						}
					}

					{
						GraphicsControlBlock gcb = 
						{
							.DisposalMode = DISPOSE_DO_NOT,
							.UserInputFlag = false,
							.DelayTime = MinDelay,
							.TransparentColor = -1,
						};

						u8 ext[4];
						EGifGCBToExtension(&gcb, ext);
						EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, sizeof ext, ext);
					}

					ColorMapObject* colorMap = GifMakeMapObject(PalSize, NULL);
					memset(colorMap->Colors, 0, PalStructSize);
					memcpy(colorMap->Colors, palette, colors * sizeof(GifColorType));

					if(EGifPutImageDesc(gif, 0, 0, swidth, sheight, false, colorMap) != GIF_ERROR)
					{
						for(s32 y = 0; y < height; y++)
						{
							for(s32 x = 0, pos = y*width; x < width; x++, pos++)
							{
								u8 color = screen[pos];
								for(s32 s = 0, pos = x*scale; s < scale; s++, pos++)
									line[pos] = color;
							}

							for(s32 s = 0; s < scale; s++)
							{
								if (EGifPutLine(gif, line, swidth) == GIF_ERROR)
								{
									error = gif->Error;
									break;
								}
							}

							if(error != E_GIF_SUCCEEDED) break;
						}

						*size = output.pos;

						result = error == E_GIF_SUCCEEDED;
					}

					GifFreeMapObject(colorMap);

					if(!result)
						break;
				}

				free(line);
				free(screen);
				free(palette);
			}
		}

		EGifCloseFile(gif, &error);

		*size = output.pos;
	}

	return result;
}