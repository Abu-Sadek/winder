#include "stepper.h"

#include <stdint.h>
#include <string.h>

#include "winder_pins.h"

#define STEP_DELAY_MICROS	30

//#define SETBIT(b, bi, v) b = (0xf & ~(1 << bi)) | (v << bi)
#define STEPPER_SETBIT(b, bi, v) b = (b & (0xff & ~(1 << bi))) | ((v & 1) << bi)
#define SET_STEPPER_FLAG_DIRX(b, v) STEPPER_SETBIT(b->flags, 0, v)
#define SET_STEPPER_FLAG_DIRY(b, v) STEPPER_SETBIT(b->flags, 1, v)
#define SET_STEPPER_FLAG_DONE(b, v) STEPPER_SETBIT(b->flags, 2, v)
#define SET_STEPPER_FLAG_READY(b, v) STEPPER_SETBIT(b->flags, 3, v)
#define SET_STEPPER_FLAG_RUN(b, v) STEPPER_SETBIT(b->flags, 4, v)

#define STEPPER_GETBIT(b, bi) ((b & (1 << bi)) >> bi)
#define STEPPER_FLAG_DIRX(b) STEPPER_GETBIT(b->flags, 0)
#define STEPPER_FLAG_DIRY(b) STEPPER_GETBIT(b->flags, 1)
#define STEPPER_FLAG_DONE(b) STEPPER_GETBIT(b->flags, 2)
#define STEPPER_FLAG_READY(b) STEPPER_GETBIT(b->flags, 3)
#define STEPPER_FLAG_RUN(b) STEPPER_GETBIT(b->flags, 4)

#define STEPPER_MAX_BUFF_SIZE	1 

int32_t abs_i(int32_t x) {
	return x > 0 ? x : -x;
}

inline int32_t max_i(int32_t x, int32_t y) {
	return x > y ? x : y;
}

struct StepperSeg
{
	uint16_t segNum ;
	
	int32_t dx ;
	int32_t dy ;
	int32_t err ;
	int32_t tMax ;
	
	volatile uint8_t flags ; 
	// bit 0 : dirx, bit 1: diry, bit 2: isDone, bit 3: isReady - block is filled with data to be executed, bit 4: isRun - block is being executed by stepper runner;
	
	volatile uint16_t rcount ; // repeat count
	volatile int32_t xCounter, yCounter ;	
} ;

StepperSeg stepperBuff2[STEPPER_MAX_BUFF_SIZE] ;


volatile StepperSeg *execSeg = &stepperBuff2[0] ;

volatile uint16_t stepper_execBuffHeadIndex = 0 ;
volatile uint16_t stepper_execBuffTailIndex = 0 ;

uint16_t stepper_maxSegNum = 1 ;

uint16_t stepper_currentSegNum = 0 ;

volatile uint8_t isMotorEnabled = 0 ;
volatile uint8_t isPulseCycle = 0 ;


void stepper_incBuffTailIndex()
{
	stepper_execBuffTailIndex++ ;
	stepper_execBuffTailIndex = stepper_execBuffTailIndex % STEPPER_MAX_BUFF_SIZE ;
}

void stepper_init()
{
	memset(&stepperBuff2, 0, sizeof(stepperBuff2));
	stepper_execBuffTailIndex = stepper_execBuffHeadIndex = 0 ;
	execSeg = &stepperBuff2[0] ;
	for(int i=0;i<STEPPER_MAX_BUFF_SIZE;++i)
	{
		SET_STEPPER_FLAG_DONE((&stepperBuff2[i]), 1) ;
	}
}

int32_t gcd(int32_t m, int32_t n)     	
{
   int32_t  r;                
   while (n != 0) 
   {      
      r = m % n;          
      m = n;            
      n = r;
   }                      
   return m;              
}

bool stepper_canAddLine()
{
	StepperSeg *seg = &stepperBuff2[stepper_execBuffTailIndex] ;
	if(!STEPPER_FLAG_DONE(seg)) // no room for new segments
		return false ;
	return true ;
}

uint16_t stepper_addLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
	StepperSeg *seg = &stepperBuff2[stepper_execBuffTailIndex] ;
	if(!STEPPER_FLAG_DONE(seg)) // no room for new segments
		return 0 ;
	stepper_incBuffTailIndex() ;
	seg->segNum = stepper_maxSegNum++ ;
	seg->flags = 0 ;
	seg->dx =  abs_i(x1-x0) ; SET_STEPPER_FLAG_DIRX(seg, (x0<x1 ? 1 : 0));
	seg->dy = -abs_i(y1-y0) ; SET_STEPPER_FLAG_DIRY(seg, (y0<y1 ? 1 : 0)); 
	seg->tMax = max_i(abs_i(seg->dx), abs_i(seg->dy)) ;
	if(seg->dy != 0 && seg->dx != 0)
	{
		seg->rcount = gcd(seg->dx, -seg->dy) ;
		seg->dx /= seg->rcount ;
		seg->dy /= seg->rcount ;
	}
	else
		seg->rcount = 1 ;
	seg->err = seg->dx+seg->dy ; /* error value e_xy */
	seg->xCounter = seg->yCounter = 0 ; 
	SET_STEPPER_FLAG_DONE(seg, 0) ;
	SET_STEPPER_FLAG_READY(seg, 1) ;
	SET_STEPPER_FLAG_RUN(seg, 0) ;
	
	isMotorEnabled = 1 ;
	return seg->segNum ;
}

/*
source: https://gist.github.com/Wollw/3291916
void plotLine(int x0, int y0, int x1, int y1)
{
	int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
	int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1; 
	int err = dx+dy, e2; // error value e_xy

	for(;;){  // loop
		setPixel(x0,y0);
		if (x0==x1 && y0==y1) break;
		e2 = 2*err;
		if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0 
		if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0 
	}
}

other implementations:
	https://news.ycombinator.com/item?id=4352943
*/

volatile uint32_t xSteps ;
volatile int32_t ySteps ;
volatile int yStep_dir ;
volatile uint32_t tCounter ;
volatile uint32_t tMax ;

int32_t stepper_YPos()
{
	return ySteps ;
}


int16_t stepper_reportCurrentSeg()
{
	return stepper_currentSegNum ;
}

uint32_t stepper_reportCurrentPos(uint32_t *maxValue)
{
	if(maxValue != 0)
		*maxValue = tMax ;
	return tCounter ;	
}

void stepper_run()
{
	if(isMotorEnabled)
	{
		execSeg = &stepperBuff2[stepper_execBuffHeadIndex] ;
		if(!STEPPER_FLAG_RUN(execSeg))
		{
			if(!STEPPER_FLAG_READY(execSeg))
			{
				// no buffers ready yet for processing, nothing to do
				//std::cout << "no buffers ready to execute" << std::endl;
				//Serial.println("no lines to process") ;
				return ;
			}
			// set direction pins for x and y
			/* pulse dir bits */
			if(STEPPER_FLAG_DIRX(execSeg))
				SET_X_DIR_ON ;
			else
				SET_X_DIR_OFF ;
			if(STEPPER_FLAG_DIRY(execSeg))
			{
				SET_Y_DIR_ON ;
				yStep_dir = 1 ;
			}
			else
			{
				SET_Y_DIR_OFF ;
				yStep_dir = -1 ;
			}
			stepper_currentSegNum = execSeg->segNum ;
			//std::cout << " iDir: x: " << (int)(STEPPER_FLAG_DIRX(execSeg)) << " y: " << (int)(STEPPER_FLAG_DIRY(execSeg)) << std::endl ;
			SET_STEPPER_FLAG_RUN(execSeg, 1) ;
			tCounter = xSteps = ySteps = 0 ;
			tMax = execSeg->tMax ;
			execSeg->xCounter = execSeg->yCounter = 0 ; //-execSeg->dy ;
			execSeg->err = execSeg->dx+execSeg->dy ;
		}
		/*Serial.print("rcount: ") ;
		Serial.print(execSeg->rcount) ;
		Serial.print("xCounter: ") ;
		Serial.print(execSeg->xCounter) ;
		Serial.print("dx: ") ;
		Serial.print(execSeg->dx) ;
		Serial.print("yCounter: ") ;
		Serial.print(execSeg->yCounter) ;
		Serial.print("dy: ") ;
		Serial.println(execSeg->dy) ;*/
		if (execSeg->xCounter == execSeg->dx && execSeg->yCounter == -execSeg->dy) 
		{
			execSeg->rcount-- ;
			if(execSeg->rcount > 0)
			{
				execSeg->xCounter = execSeg->yCounter = 0 ; //-execSeg->dy ;
				execSeg->err = execSeg->dx+execSeg->dy ;
			} else {
				SET_STEPPER_FLAG_DONE(execSeg, 1) ;
				SET_STEPPER_FLAG_RUN(execSeg, 0) ;
				SET_STEPPER_FLAG_READY(execSeg, 0) ;
				stepper_execBuffHeadIndex++ ;
				stepper_execBuffHeadIndex = stepper_execBuffHeadIndex % STEPPER_MAX_BUFF_SIZE ;

				execSeg = &stepperBuff2[stepper_execBuffHeadIndex] ;
				if(STEPPER_FLAG_READY(execSeg))
				{
					execSeg->xCounter = execSeg->yCounter = 0 ; //-execSeg->dy ;
					execSeg->err = execSeg->dx+execSeg->dy ;
					/* pulse dir bits */
					if(STEPPER_FLAG_DIRX(execSeg))
						SET_X_DIR_ON ;
					else
						SET_X_DIR_OFF ;
					if(STEPPER_FLAG_DIRY(execSeg))
					{
						SET_Y_DIR_ON ;
						yStep_dir = 1 ;
					}
					else
					{
						SET_Y_DIR_OFF ;
						yStep_dir = -1 ;
					}
					//std::cout << " Dir: x: " << (int)(STEPPER_FLAG_DIRX(execSeg)) << " y: " << (int)(STEPPER_FLAG_DIRY(execSeg)) << std::endl ;
					stepper_currentSegNum = execSeg->segNum ;
					SET_STEPPER_FLAG_RUN(execSeg, 1) ;
					SET_STEPPER_FLAG_DONE(execSeg, 0) ;
					tCounter = xSteps = ySteps = 0 ;
					tMax = execSeg->tMax ;
				} else {
					stepper_currentSegNum = 0 ;
					tMax = 0 ;
					isMotorEnabled = 0 ;
				}
			}
		}
		if(isMotorEnabled)
		{
			int e2 = execSeg->err << 1;
			if (e2 >= execSeg->dy) 
			{ 
				execSeg->err += execSeg->dy; 
				//std::cout << stepper_execBuffHeadIndex << ": X-Step " << xSteps << std::endl ;/* pulse x */ 
				SET_X_STEP_ON ;
				execSeg->xCounter++ ; /*x0 += sx;*/ 
				xSteps++ ;
			} /* e_xy+e_x > 0 */
			if (e2 <= execSeg->dx) 
			{ 
				execSeg->err += execSeg->dx; 
				//std::cout << stepper_execBuffHeadIndex << ": Y-Step --" << ySteps << std::endl ;/* pulse y */ 
				SET_Y_STEP_ON ;
				execSeg->yCounter++ ; /*y0 += sy;*/ 
				ySteps += yStep_dir ;
			} /* e_xy+e_y < 0 */
			tCounter++ ;
			//std::cout << " xCounter: " << execSeg->xCounter << " yCounter: " << execSeg->yCounter << ", dy: " << execSeg->dy << std::endl ;
			delayMicroseconds(STEP_DELAY_MICROS) ;
			SET_X_STEP_OFF ;
			SET_Y_STEP_OFF ;
		}
	}
}
