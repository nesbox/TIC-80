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
#include <string.h>
#include "studio/project.h"

struct Args {
    char* project;
    char* cartridge;
    char* binarychunk;
} args;

int res = -1;

void processArgs(int argc, char** argv) {
    args.project = argv[1];
    args.cartridge = argv[2];

    if (argv[3] && strncmp(argv[3],"--binary", 8) == 0) {
        if (!argv[4]) {
            printf("--binary specified, but no filename given");
            res = -1;
        }
        args.binarychunk = argv[4];
    }
}

char * readFile(char* name, unsigned int* size) {
    FILE* file = fopen(name, "rb");

    if(!file) {
        printf("cannot open file file\n");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(*size);

    fread(buffer, *size, 1, file);
    fclose(file);

    return buffer;
}

int main(int argc, char** argv)
{
    processArgs(argc, argv);

    if (!args.project || !args.cartridge) {
        printf("usage: wasmp2cart <project> <cartridge> [--binary file.wasm]\n");
        return res;
    }

    int size;
    char* project_contents = readFile(args.project, &size);
    if (!project_contents) return res;

    tic_cartridge* cart = calloc(1, sizeof(tic_cartridge));

    tic_project_load(args.project, project_contents, size, cart);

    FILE* cartFile = fopen(args.cartridge, "wb");

    if(!cartFile) {
        printf("cannot open cartridge file\n");
        free(project_contents);
        free(cart);
        return res;
    }

    if (args.binarychunk) {
        int wasm_size;
        char* wasm_contents = readFile(args.binarychunk, &wasm_size);
        if (!wasm_contents) {
            printf("could not load WASM file\n");
            return res;
        }
        memcpy(cart->binary.data, wasm_contents, wasm_size);
        cart->binary.size = wasm_size;
        free(wasm_contents);
    }

    unsigned char* out = (unsigned char*)malloc(sizeof(tic_cartridge));

    if(out)
    {
        int outSize = tic_cart_save(cart, out);

        fwrite(out, outSize, 1, cartFile);

        free(out);
    }

    fclose(cartFile);

    res = 0;

    free(project_contents);
    free(cart);

    return res;
}
