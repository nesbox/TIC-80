#include "utils.h"


#define MAXBUFLEN (1024*1024)


extern "C" {

// this functions are absent for circle-stdlib for some reason
// (maybe i can't link properly). Since they shouldn't be used
// too much, i'll put some stub here. Chmod is ok since we're on 
// fat anyway, symlink won't work either. ftruncate could break
// something

int chmod (const char *filename, mode_t mode)
{
dbg("called chmod\n");
	return -1;
}

int ftruncate(int fd, off_t length)
{
dbg("called ftruncate\n");
	return -1;
}

int symlink(const char *target, const char *linkpath)
{
dbg("called symlink\n");

	return -1;
}
}

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

