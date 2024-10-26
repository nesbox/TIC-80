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
#include <libgen.h>
#include <string.h>
#include <errno.h>

#include "studio/project.h"

FILE *open_project(char *project_path);
void usage(void);
void convert_project_to_cartridge(char *project_path,
																	char *output_cartridge_path);

int main(int argc, char **argv) {
  if (strcmp(argv[1], "-i") != 0 && argc != 4) {
    usage();
  } else {
    if (strcmp(argv[1], "-i") == 0) {
			int outputDirectory = 3;
			while(strcmp(argv[outputDirectory++], "-o") != 0) {
				if (outputDirectory <= argc - 2) {
					continue;
				} else {
					printf("outputDirectory argument (-o exampleDirectory) not in command line or argument value missing.");
					exit(EX_USAGE);
				}
			}
			char *outputFilename;
			/* NOTE: I do not fully understand why I needed to subtract one from outputDirectory. */
			for (int inputFile = 2; inputFile < outputDirectory - 1; inputFile++) {
				/* Construct the output filename using the basename of the inputFile and
				 * the outputDirectory. */
				size_t size = strlen((const char *) argv[outputDirectory])
					+ strlen((const char *) basename(argv[inputFile]));
				outputFilename = malloc(size);
				sprintf(outputFilename, "%s/", (const char *) argv[outputDirectory]);
				size_t acceptSize = strcspn((const char *) basename(argv[inputFile]), (const char *) ".");
				strncpy(outputFilename + strlen(outputFilename),
								(const char *) basename(argv[inputFile]),
								acceptSize);
				strcat(outputFilename, (const char *) ".tic");
				/* prj2cart -i file1 file2 file3 -o outputDirectory */
				convert_project_to_cartridge(argv[inputFile], outputFilename);
				memset(outputFilename, 0, strlen(outputFilename));
				free(outputFilename);
			}
    } else {
			/* prj2cart inputFilename outputFilename */
      convert_project_to_cartridge(argv[1], argv[2]);
    }
    exit(EX_OK);
  }
}

FILE *open_project(char *project_path) {
  FILE *project;
  if (!(project = fopen(project_path, "rb"))) {
    printf("Cannot open project file %s for reading\n\n", project_path);
    exit(EX_NOINPUT);
  } else {
    return project;
  }
}

void usage(void) {
  printf("usage: prj2cart <project> <cartridge>\n"
         "       prj2cart -i <project>...\n"
         "%80s\n",
         "The second form will automatically strip a file extension and "
         "replace it with \".tic\", and loop over project files, creating "
         "multiple cartridges.");
  exit(EX_USAGE);
}

void convert_project_to_cartridge(char *project_path,
																	char *output_cartridge_path) {
  if (output_cartridge_path != NULL
      && strcmp(basename(project_path),
                basename(output_cartridge_path)) == 0
			|| strcmp(basename(output_cartridge_path), "") == 0) {
		printf("Project path or output cartridge path is bad.");
    exit(EXIT_FAILURE);
  }
  FILE *project = open_project(project_path);
  fseek(project, 0, SEEK_END);
  int size = ftell(project);
  fseek(project, 0, SEEK_SET);

  unsigned char *buffer = (unsigned char *) malloc(size);

  if (buffer) {
    fread(buffer, size, 1, project);
    fclose(project);
    tic_cartridge *cart = calloc(1, sizeof(tic_cartridge));
    tic_project_load(project_path, (char *) buffer, size, cart);

    FILE *cartFile;

    if (output_cartridge_path == NULL) exit(EX_USAGE);
		else if ( (cartFile = fopen(output_cartridge_path, "wb")) ) {
      unsigned char *out;
      if ((out = (unsigned char *) malloc(sizeof(tic_cartridge)))) {
        int outSize = tic_cart_save(cart, out);
        fwrite(out, outSize, 1, cartFile);
				printf("Wrote cartridge file %s\n", output_cartridge_path);
        free(out);
      }
      fclose(cartFile);
		} else {
      printf("Cannot open cartridge file %s for writing. Reason: %s\n",
						 output_cartridge_path, strerror(errno));
			free(buffer);
			free(cart);
			exit(EX_CANTCREAT);
    }

		free(buffer);
		free(cart);
	}
}
