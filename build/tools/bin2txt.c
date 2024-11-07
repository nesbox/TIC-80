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
#include <stdbool.h>
#include <string.h>
#include <zlib.h>

int main(int argc, char** argv)
{
	int res = -1;

	if(argc >= 3)
	{
		bool useZip = false;

		if(argc == 4 && strcmp(argv[3], "-z") == 0) useZip = true;

		FILE* bin = fopen(argv[1], "rb");

		if(bin)
		{
			fseek(bin, 0, SEEK_END);
			int size = ftell(bin);
			fseek(bin, 0, SEEK_SET);

			unsigned char* buffer = (unsigned char*)malloc(size);

			if(buffer)
			{
				fread(buffer, size, 1, bin);
				fclose(bin);

				if(useZip)
				{
					unsigned char* output = (unsigned char*)malloc(size);

					if(output)
					{
						unsigned long sizeComp = size;
						if(compress2(output, &sizeComp, buffer, size, Z_BEST_COMPRESSION) != Z_OK)
						{
							printf("compression error\n");
						}
						else
						{
							size = sizeComp;
							memcpy(buffer, output, size);
						}

						free(output);						
					}
					else printf("memory error :(\n");
				}

				FILE* txt = fopen(argv[2], "wb");

				if(txt)
				{
					for(int i = 0; i < size; i++)
						fprintf(txt, "0x%02x, ", buffer[i]);

					fclose(txt);

					res = 0;
				}
				else printf("cannot open text file\n");

				free(buffer);
			}

		}
		else printf("cannot open bin file\n");
	}
	else printf("usage: bin2txt <bin> <txt>\n");

	return res;
}
