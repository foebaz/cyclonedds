
#include <stdio.h>
#include <stdlib.h>
#include "idl/streamer_generator.h"
#include "idl/backend.h"
#include "idl/processor.h"

int main(int argc, char **argv)
{
  printf("testing\n");

  char* source = NULL;
  FILE* fp = fopen(argv[1], "r");

  if (NULL == fp)
    return -1;

  if (fseek(fp, 0L, SEEK_END) == 0) {
    /* Get the size of the file. */
    long bufsize = ftell(fp);
    if (bufsize == -1) { /* Error */ }

    /* Allocate our buffer to that size. */
    source = malloc(sizeof(char) * (bufsize + 1));

    /* Go back to the start of the file. */
    if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

    /* Read the entire file into memory. */
    size_t newLen = fread(source, sizeof(char), bufsize, fp);
    if (ferror(fp) != 0) {
      fputs("Error reading file", stderr);
    }
    else {
      source[newLen++] = '\0'; /* Just to be safe. */
    }
  }
  fclose(fp);

  idl_tree_t* tree = 0x0;
  idl_parse_string(source, 0x0, &tree);

  idl_streamers_generate(source, argv[1]);
  free(source);

  return 0;
}