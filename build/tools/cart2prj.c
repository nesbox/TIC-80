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

#include <stdio.h>
#include <stdlib.h>
#include "studio/project.h"

int main(int argc, char** argv)
{
	int res = -1;

	if(argc == 3)
	{
		FILE* cartFile = fopen(argv[1], "rb");

		if(cartFile)
		{
			fseek(cartFile, 0, SEEK_END);
			int size = ftell(cartFile);
			fseek(cartFile, 0, SEEK_SET);

			unsigned char* buffer = (unsigned char*)malloc(size);

			if(buffer)
			{
				fread(buffer, size, 1, cartFile);
				fclose(cartFile);

				tic_cartridge* cart = calloc(1, sizeof(tic_cartridge));

				tic_cart_load(cart, buffer, size);

				FILE* project = fopen(argv[2], "wb");

				if(project)
				{
					unsigned char* out = (unsigned char*)malloc(sizeof(tic_cartridge) * 3);

					if(out)
					{
						s32 outSize = tic_project_save(argv[2], out, cart);

						fwrite(out, outSize, 1, project);

						free(out);
					}

					fclose(project);

					res = 0;
				}
				else printf("cannot open project file\n");

				free(buffer);
				free(cart);
			}

		}
		else printf("cannot open cartridge file\n");
	}
	else printf("usage: cart2prj <cartridge> <project>\n");

	return res;
}
