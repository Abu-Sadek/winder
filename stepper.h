#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>

void stepper_init() ;

uint16_t stepper_addLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1) ;

bool stepper_canAddLine() ;

void stepper_run() ;

int32_t stepper_YPos() ;

uint32_t stepper_reportCurrentPos(uint32_t *maxValue) ;

void stepper_pause() ;
void stepper_resume() ;


#undef M_PI
#define M_PI 3.14159265358979323846f
#define RADIANS(d) ((d)*M_PI/180.0f)
#define DEGREES(r) ((r)*180.0f/M_PI)
#define HYPOT2(x,y) (sq(x)+sq(y))

#define CIRCLE_AREA(R) (M_PI * sq(float(R)))
#define CIRCLE_CIRC(R) (2 * M_PI * (float(R)))

#define SIGN(a) ((a>0)-(a<0))
#define IS_POWER_OF_2(x) ((x) && !((x) & ((x) - 1)))


#endif
