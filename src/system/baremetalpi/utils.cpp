#include "utils.h"


#define MAXBUFLEN (1024*1024)

char *strdup (const char *s) {
    char *d = (char*)malloc (strlen (s) + 1);   // Space for length plus nul
    if (d == NULL) return NULL;          // No memory
    strcpy (d,s);                        // Copy the characters
    return d;                            // Return the new string
}



void* loadFile(const char *filename, u32* size)
{
/*
  // slow way to load a file since we don't have fseek
/  char source[MAXBUFLEN];
  FILE *fp = fopen(filename, "rb");
  if (fp != NULL) {
    size_t newLen = fread(source, sizeof(char), MAXBUFLEN, fp);
    if ( ferror( fp ) != 0 ) {
        fputs("Error reading file", stderr);
    } else {
    }

    fclose(fp);
    printf("STREAM SIZE is %d\n", newLen);
    void* cart = malloc(newLen);
    memcpy(cart, source, newLen);
    *size = newLen;
    return cart;
  }
  else
  {
    return NULL;
  }
*/
return NULL;
}

