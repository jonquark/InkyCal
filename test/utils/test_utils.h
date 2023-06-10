#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "test_utils_assert.h"

//The returned buffer needs to be freed after use
char *test_utils_fileToString(const char *filename);

#endif //TEST_UTILS_H