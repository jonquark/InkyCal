//Designed to be included in most C files in this project ...
//so it should include the minimum number of other headers possible

#ifndef INKYCALINTERNAL_H
#define INKYCALINTERNAL_H

#include <stdint.h>

#define INKY_SEVERITY_WARNING 10
#define INKY_SEVERITY_ERROR   20
#define INKY_SEVERITY_FATAL   30  //Probably won't see these on screen as wont get far enough
                                  //   (could change this to draw error on screen?)
 //At end of processing we record number of errors
void logProblem(uint32_t severity);//Currently in InkyCal.ino

#endif