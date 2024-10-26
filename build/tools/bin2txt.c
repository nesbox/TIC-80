// MIT License

// Copyright (c) 2024 Bryce Carson @bryce-carson // bryce.a.carson@gmail.com
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
#include <sysexits.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h>
#include <zlib.h>

void convert_cartridge_to_text(char *cartridge_path, char *text_path, bool useZip);
void usage(void);
void with_zip(int size, unsigned char *buffer);

int main(int argc, char** argv) {
	/* bin2txt -zi inputFile... -o outputDirectory */
  if (argc >= 3 && (strcmp(argv[1], "-iz") == 0 || strcmp(argv[1], "-zi") == 0)) {
		int outputDirectory = 3;
		/* While the output option hasn't been located, check that we haven't
		 * overrun the bounds of the argument vector. */
		while(strcmp(argv[outputDirectory++], "-o") != 0) {
			/* Prevent an infinite loop; ensure the -o option is given
			 * eventually. */
			if (outputDirectory <= argc - 2) {
				continue;
			} else {
				printf("-o was not found in the command line or in the correct position (is the argument value missing?).");
				exit(EX_USAGE);
			}
		}
		/* bin2txt -zi inputFile... -o outputDirectory */
    for (int inputFile = 2; inputFile < outputDirectory - 1; inputFile++) {
			char *outputFile = malloc(strlen(".dat") + strlen(argv[outputDirectory]) + strlen(basename(argv[inputFile])));
			sprintf(outputFile, "%s/", argv[outputDirectory]);
			strcat(outputFile, (const char *) basename(argv[inputFile]));
			strcat(outputFile, ".dat");
      convert_cartridge_to_text(argv[inputFile], outputFile, true);
			memset(outputFile, 0, strlen(outputFile));
			free(outputFile);
    }
  } else if (argc == 4 && strcmp(argv[3], "-z") == 0) {
    convert_cartridge_to_text(argv[1], argv[2], true);
  } else if (argc == 3) {
    convert_cartridge_to_text(argv[1], argv[2], false); /* no zlib */
  } else {
    usage();
  }
}

void usage(void) {
  printf("usage: bin2txt <input cartridge file> <output text file> [-z]\n"
         "       bin2txt [-zi] <cartridge file1>... -o <text file output directory>\n"
         "%80s\n"
         "The second form will use zlib compression and automatically generate "
         "output filenames for multiple input cartridges.");
  exit(EX_USAGE);
}

void convert_cartridge_to_text(char *cartridge_path, char *text_path, bool useZip) {
  FILE *bin;
  if (!(bin = fopen(cartridge_path, "rb"))) {
    printf("Cannot open bin file %s for reading.\n", cartridge_path);
    exit(EX_NOINPUT);
  } else {
		/* Get the size of the file by seeking through it, rather than other means. */
    fseek(bin, 0, SEEK_END);
    int size = ftell(bin);
    fseek(bin, 0, SEEK_SET);

    unsigned char *buffer;
    if (!(buffer = (unsigned char *) malloc(size))) {
			printf("Could not successfully allocate memory to convert cartridge to text.");
			exit(EX_IOERR); /* if cannot create the buffer below. */
    } else {
			fread(buffer, size, 1, bin);
			fclose(bin); /* close cartridge_path */

			if (useZip) with_zip(size, buffer);

			FILE *txt;
			if ((txt = fopen(text_path, "wb"))) {
				for (int i = 0; i < size; i++) fprintf(txt, "0x%02x, ", buffer[i]);
				fclose(txt);
			} else {
				printf("Cannot open text file %s for writing.\n", text_path);
				exit(EX_CANTCREAT);
			}

			free(buffer);
    }
  }
}

void with_zip(int size, unsigned char *buffer) {
  unsigned char *output = (unsigned char *) malloc(size);

  if (output) {
    unsigned long sizeComp = size;

		if(compress2(output, &sizeComp, buffer, size, Z_BEST_COMPRESSION) != Z_OK) {
      printf("compression error\n");
    }
    else {
      size = sizeComp;
      memcpy(buffer, output, size);
    }

    free(output);
  } else {
    printf("memory error :(\n");
    exit(EX_CANTCREAT);
  }
}
