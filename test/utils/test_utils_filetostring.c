#include <stdio.h>
#include <stdlib.h>

#include "test_utils_assert.h"

//The returned buffer needs to be freed after use
char *test_utils_fileToString(const char *filename)
{
    char *buffer = NULL;

    FILE *f = fopen(filename, "r");
    TEST_ASSERT_PTR_NOT_NULL(f);
    
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long length = ftell (f);
        fseek(f, 0, SEEK_SET);
  
        buffer = (char *)malloc(length+1);
        if (buffer)
        {
            fread(buffer, 1, length, f);
            buffer[length] = '\0';
        }
        fclose(f);
    }

    return buffer;
}
