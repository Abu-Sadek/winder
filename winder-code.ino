//#include <iostream>

#define UNOCNC_SHIELD	1
#define RAMPS_1_4

#define MAINBOARD	UNOCNC_SHIELD

#include <Arduino.h>
#include "winder_pins.h"
#include "winder-parser.h"
#include "stepper.h"
#include "readline.h"

#include "winder-screen.h"

#include <LiquidCrystal_I2C.h>

#include "screen-ilcd.h"

#include <TimerOne.h>

#include "input_rotaryEncoderWButton.h"

#include "PinChangeInt/PinChangeInt.h"

typedef unsigned char byte ;

// maximum system speed to avoid timers with less than 100 micro seconds winding 
#define MAX_SYSTEM_SPEED 125.0f

//#define COMMANDS_FROM_SERIAL

#define USE_ENDSTOPS

class HWScreen : public ILcd, public LiquidCrystal_I2C
{
public:
	HWScreen(uint8_t lcd_addr,uint8_t lcd_cols,uint8_t lcd_rows) : LiquidCrystal_I2C(lcd_addr, lcd_cols, lcd_rows)
	{

	}

    void setCursor(uint8_t x, uint8_t y) { LiquidCrystal_I2C::setCursor(x, y) ; } ;
    void begin(uint8_t cols, uint8_t rows) { LiquidCrystal_I2C::begin(/*cols, rows*/) ; } ;
    void clear() { LiquidCrystal_I2C::clear() ; } ;
	void backlight() { LiquidCrystal_I2C::backlight() ; } ;

    size_t print(const char v[]) { return LiquidCrystal_I2C::print(v) ; } ;
    size_t print(char v) { return LiquidCrystal_I2C::print(v) ; } ;
    size_t print(unsigned char v, int rad = DEC) { return LiquidCrystal_I2C::print(v, rad) ;} ;
    size_t print(int v, int rad = DEC) { return LiquidCrystal_I2C::print(v, rad) ; } ;
    size_t print(unsigned int v, int rad = DEC) { return LiquidCrystal_I2C::print(v, rad) ; } ;
    size_t print(long v, int rad = DEC) { return LiquidCrystal_I2C::print(v, rad) ; } ;
    size_t print(unsigned long v, int rad = DEC) { return LiquidCrystal_I2C::print(v, rad) ; } ;
    size_t print(double v, int rad = 2) { return LiquidCrystal_I2C::print(v, rad) ; } ;
    size_t print(const Printable& v) { return LiquidCrystal_I2C::print(v) ; } ;
} ;

#ifdef COMMANDS_FROM_SERIAL
class SerialReader : public IByteInput
{
public:
	int avail() { return Serial.available() ; }
	int read() { return Serial.read() ; }
} ;

SerialReader _serialReader ;

ReadLine _readline(&_serialReader) ;
#endif

HWScreen lcd(0x27, LCD_SIZE_COLS, LCD_SIZE_ROWS);

float systemRollingSpeed() ;
void systemSetRollingSpeed(float mmSec) ;

int rollStepsPerRev = 9600 ;  // 600 * 32
int pointingStepsPerMM = 2560 ; // 200 * 32 / 5 * 4 // 2560 / 1.25 ; 200pulse/rev with M8 1.25mm pitch

int dir = 0 ; // 0 - left, 1 - right

// inputs: es = extudeSpeedMM/Sec, srd = startRollingDiameterInMM, rrw=remainingRollingWidthInMM, 
// 			frw = FullRollingWidthInMM, erd = endRollingDiameterInMM
// output: rsr = rollingStepsRemaining, psr = pointingStepsRemaining
long rsr = 1 ;
long psr = 1 ;
//float rfl = 1 ;
float revLen ; // rrl = remainingRollingLength for remainingRollingWidth in MM

// Command buffer vars
#define COMMAND_BUFF_SIZE  2
Command commandBuff[COMMAND_BUFF_SIZE] ;
byte commandBuff_consumeIndex = 0, commandBuff_produceIndex = 0;

// vars used for next layer of processWindingCommand
Command *currentCommand = 0;
float nextStartDiam ;
float nextHDist ; 
float targetDiam ;
float startingSpeed ;
float totalWidth ;
byte nextDir ;
byte currentDir = 0 ;

// params of move pointer command
float mp_dist ;
uint8_t mp_dir ;

float pointingSpeed = 5.0f ;

// next command object
Command *freeComm = 0 ;

// system rolling speed
float _system_rollingSpeed = 50.0f;

// the following vars are used by winderoption_* functions
float mv_startDiam = 85.0f ;
float mv_endDiam = 185.0f ;
float mv_startWidth = 60.0f ;
float mv_totalWidth = 60.0f ;
float mv_runHours = 7.5f;
bool mv_isDirLeft = true ;

// used by winder_getGuidePos
float wp_change = 0 ;


void calcCurrentWindingLayerParams(float srd, float rrw)
{
	rsr = (float)rollStepsPerRev * (float)((int)(rrw / 1.75)) ;
	psr = pointingStepsPerMM * rrw ;
	revLen = srd * 3.14 ;
	//rfl = srd * 3.14 * rrw / 1.75 ;
}

// input: es = extrudeSpeedMM, rsr = rollingStepsRemaining, psr = pointingStepsRemaining
float calcStepperRollingTimerInterval(float es) // extrusion speed in mm/sec
{
	float rollPulsesPerSec = es / revLen * rollStepsPerRev ;	
  float f , f2 ;
  if(abs(psr) > abs(rsr))
  {
    f = rollPulsesPerSec ;
    f2 = psr ;
    f2 = f2 / (float)rsr;
    f *= f2 ;
  }
  else
    f = rollPulsesPerSec ;
	float timerInterval = 1000000 / f ;
 if(timerInterval < 0)
  timerInterval *= -1.0 ;
	//setTimer(timerInterval, timerFunc) ;
	return timerInterval ;
}


void commandBuff_init()
{
	commandBuff_consumeIndex = commandBuff_produceIndex = 0 ;
	for(int i=0;i<COMMAND_BUFF_SIZE;++i)
	{
		commandBuff[i].isFree = 1 ;
		commandBuff[i].isInExec = 0 ;
	}
}

Command *commandBuff_allocEntry()
{
	if(!commandBuff[commandBuff_produceIndex].isFree)
	{
		return 0 ;
	}
	
	auto comm = &commandBuff[commandBuff_produceIndex] ;
	comm->isFree = 0 ;
	commandBuff_produceIndex++;
	commandBuff_produceIndex %= COMMAND_BUFF_SIZE ;
	return comm ;
}

Command *commandBuff_consume()
{
	if(commandBuff[commandBuff_consumeIndex].isFree)
		return 0 ;
	auto comm = &commandBuff[commandBuff_consumeIndex] ;
	commandBuff_consumeIndex++ ;
	commandBuff_consumeIndex %= COMMAND_BUFF_SIZE ;
	return comm ;
}

void commandBuff_freeEntry(Command *comm)
{
	comm->isFree = 1 ;
	comm->isInExec = 0 ;
}

bool commandBuff_isEmpty()
{
	if(commandBuff_consumeIndex == commandBuff_produceIndex 
		&& commandBuff[commandBuff_consumeIndex].isFree)
		return true ;
	return false ;
}

bool commandBuff_hasFree() // has free entry
{
	if(commandBuff_isEmpty())
		return true ;
	if(commandBuff_consumeIndex != commandBuff_produceIndex 
		&& commandBuff[commandBuff_produceIndex].isFree)
		return true ;
	return false ;
}

// the distance in mm that will be added to each side of the total filament guide range
//float wind_pointerRangeInc = 4 ;

// returns true if finished
bool processWindingCommand(Command *comm)
{
	if(!comm->isInExec)
	{
		nextStartDiam = comm->data.wind.startDiam ;
		nextHDist = comm->data.wind.startWidth ;
		targetDiam = comm->data.wind.targetDiam ;
		totalWidth = comm->data.wind.totalWidth ;
		//startingSpeed = comm->data.wind.speed ;
		nextDir = comm->data.wind.direction ;
		comm->isInExec = 1 ;
    Serial.println("New wind") ;
	}
	
	if(!stepper_canAddLine())
		return false ;
	
	// set stepper timer
	// add stepper line 
	calcCurrentWindingLayerParams(nextStartDiam, nextHDist) ;
	float interval = calcStepperRollingTimerInterval(systemRollingSpeed()) ;
	systemSetTimerInterval(interval) ;
	long x0, y0, x1, y1 ;
	currentDir = nextDir ;
  x0 = 0 ;
  x1 = rsr ;
	if(nextDir) // 0 - left , 1 - right. default dir is moving away from pointing motor 
	{
		y0 = psr ;
		y1 = 0 ;
	} else {
		y0 = 0 ; 
		y1 = psr ;
	}

	if(stepper_addLine(x0, y0, x1, y1))
	{	
    /*Serial.print("interval: ") ;
    Serial.print((int)interval) ;
    Serial.print(", rstepsRev: ") ;
    Serial.print(rollStepsPerRev) ;
    Serial.print(", rsr: ");
    Serial.print(rsr) ;
    Serial.print(", psr: ");
    Serial.print(psr) ;
    Serial.print(", revLen: ") ;
    Serial.print(revLen) ;
    Serial.print(", hdist: ") ;
    Serial.print(nextHDist) ;
    Serial.print(", diam: ") ;
    Serial.println(nextStartDiam) ;*/
    //Serial.print("add line: ") ;
/*    Serial.print(x0) ;
    Serial.print(",") ;
    Serial.print(y0) ;
    Serial.print(",") ;
    Serial.print(x1) ;
    Serial.print(",") ;
    Serial.println(y1) ;*/
		nextStartDiam += 1.75 ; // 1.75 * 2
		nextDir = nextDir ? 0 : 1 ;
		nextHDist = totalWidth ;
		if(nextStartDiam > targetDiam)
		{
      Serial.println("wind finished") ;
			comm->isInExec = 0 ;
      return true ; // command execution finished 
		}
	}
	return false ;
}

void setPointingSpeed(float v)
{
	pointingSpeed = v ;
}

float getPointingSpeed()
{
	return pointingSpeed ;
}

char ftemp[10] ;
bool processMovePointerCommand(Command *comm)
{
	if(!comm->isInExec)
	{
		mp_dist = comm->data.movep.dist ;
		mp_dir = comm->data.movep.direction ;
		comm->isInExec = 1 ;
	}
	
	if(!stepper_canAddLine())
		return false ;

  float timerInterval = 1000000.0f / (pointingStepsPerMM * pointingSpeed) ;
	systemSetTimerInterval(timerInterval) ; // 4 rpm, ~ 5mm/sec guide movement speed with 1.25 8mm rod
	
	long d, d1 = 0;
	d = (int32_t)(mp_dist * pointingStepsPerMM) ;
	currentDir = mp_dir ;
	if(!mp_dir)
	{
		d1 = d ;
		d = 0 ;
	}
 Serial.print("MVP: y0: ") ;
 Serial.print(d) ;
 Serial.print(" y1: ") ;
 Serial.print(d1) ;
 Serial.print(" d: ") ;
 Serial.print(mp_dir) ;
 Serial.print(" T:") ;
 Serial.println((int)timerInterval) ;
 if(stepper_addLine(0,d,0,d1))
	{
		comm->isInExec = 0 ;
		return true ;
	}
	return false ;
}


// returns true if finished all commands 
bool processQueueCommands()
{
	if(currentCommand == 0)
	{
		currentCommand = commandBuff_consume() ;
		if(currentCommand == 0) // no command to process 
			return true ;
	}
	
	switch(currentCommand->op)
	{
		case OP_Wind:
			if(processWindingCommand(currentCommand))
			{
				commandBuff_freeEntry(currentCommand) ;
				currentCommand = 0 ;
			}
			break ;
			
		case OP_MovePointer:
			if(processMovePointerCommand(currentCommand))
			{
				commandBuff_freeEntry(currentCommand) ;
				currentCommand = 0 ;
			}
			break ;
	}
	
	return false ;
}

void timerHandler() ;

/*	float rollDiam ;
	float hDist ;
	float targetDiam ;
	float totalHDist ;
	float speed ;
	byte direction ;
*/
bool enqueueWindCommand(float startDiam, float targetDiam, float startHDist, float totalHDist, byte startDir)
{
	Command *comm = commandBuff_allocEntry() ;
	if(comm == 0) // no room to produce
		return false ;
		
	comm->op = OP_Wind ;
	comm->data.wind.startDiam = startDiam ;
	comm->data.wind.startWidth = startHDist ;
	comm->data.wind.targetDiam = targetDiam ;
	comm->data.wind.totalWidth = totalHDist ;
	comm->data.wind.direction = startDir ;
	
	return true ;
}

bool enqueueMovePCommand(float dist, byte dir)
{
	Command *comm = commandBuff_allocEntry() ;
	if(comm == 0)
		return false ;

	comm->op = OP_MovePointer ;
	comm->data.movep.dist = dist ;
	comm->data.movep.direction = dir ;
	return true ;
}

/*bool isFirst = true ;

unsigned int steps = 0 ;
bool yDir = true ;

void loop3()
{
  if(isFirst)
  {
    isFirst = false ;
    SET_X_DIR_ON ;
    digitalWrite(UNOCNC_X_ENA_PIN, LOW) ;
  }
  if(steps >= 60000)
  {
    steps = 0 ;
    if(yDir)
    {
      SET_Y_DIR_ON ;
      yDir = false ;
    }
    else
    {
      SET_Y_DIR_OFF ;
      yDir = true ;
    }
  }
  SET_X_STEP_ON ;
  SET_Y_STEP_ON ;
  delayMicroseconds(10) ;
  SET_X_STEP_OFF ;
  SET_Y_STEP_OFF ;
  delayMicroseconds(10) ;
  delayMicroseconds(100) ;
  steps += 1 ;
}

unsigned long lastLoopTime ;
*/

/*
 * #define WINDER_LS_STOP_1  UNOCNC_HOLD
#define WINDER_LS_STOP_2  UNOCNC_X_ENDSTOP

#define WINDER_LS_RUN_1  UNOCNC_Y_ENDSTOP
#define WINDER_LS_RUN_2  UNOCNC_Z_ENDSTOP
*/

bool isSystemPaused = false ;
void system_pause()
{
  stepper_pause() ;
  isSystemPaused = true ;
}

void system_resume()
{
  stepper_resume() ;
  isSystemPaused = false ;
}
 
void setup()
{
  Serial.begin(9600) ;
  Serial.print("Starting...") ;
  lcd.begin(LCD_SIZE_COLS, LCD_SIZE_ROWS) ;
  
  pinMode(PIN_X_ENA, OUTPUT) ;
  pinMode(PIN_X_STEP, OUTPUT) ;
  pinMode(PIN_X_DIR, OUTPUT) ;

  pinMode(PIN_Y_ENA, OUTPUT) ;
  pinMode(PIN_Y_STEP, OUTPUT) ;
  pinMode(PIN_Y_DIR, OUTPUT) ;

#ifdef USE_ENDSTOPS
  pinMode(WINDER_LS_STOP_1, INPUT) ;
  pinMode(WINDER_LS_STOP_2, INPUT) ;
  pinMode(WINDER_LS_RUN_1, INPUT) ;
  pinMode(WINDER_LS_RUN_2, INPUT) ;
#endif
  
  Screen_init(&lcd) ;
  //Serial.println("done") ;

  //Serial.print("Setting up input...") ;
  input_setup() ;
  //Serial.println("done") ;
  
  //Serial.print("Starting commandBuff...") ;
  commandBuff_init() ;
  //Serial.println("done") ;

  //Serial.print("Starting stepper...") ;
  stepper_init() ;
  //Serial.println("done") ;

  //Serial.print("Starting Readline...") ;
#ifdef COMMANDS_FROM_SERIAL
  _readline.reset() ;
#endif

  Serial.println("done") ;

  /*if(!winder_isProcessingCommands())
    Serial.println("stepper OK") ;
  else
    Serial.println("stepper has flag problem") ;*/
  digitalWrite(UNOCNC_X_ENA_PIN, LOW) ;
  //delay(3000) ;
  //if(!winder_isProcessingCommands())
  //  Serial.println("stepper OK") ;

  //if(!stepper_canAddLine(0,0,0,-32000))
  if(!stepper_canAddLine())
  {
    Serial.println("no room") ;
  }

#ifdef USE_ENDSTOPS
  PCintPort::attachInterrupt(WINDER_LS_STOP_1, system_pause, FALLING);
  PCintPort::attachInterrupt(WINDER_LS_STOP_2, system_pause, FALLING);
  PCintPort::attachInterrupt(WINDER_LS_RUN_1, system_resume, FALLING);
  PCintPort::attachInterrupt(WINDER_LS_RUN_2, system_resume, FALLING);
#endif

  //else
  Timer1.initialize(100) ;
  Timer1.attachInterrupt(timerHandler/*, 16000000.0f*/) ;
  delay(1000) ;
}

unsigned long lastReportPosMillis = millis() ;

/*volatile uint32_t tcounter = 0 ;

uint32_t getCounterValue() ;
uint32_t oldCounterValue = 0;*/
unsigned long lastSystemStateMess = millis() ;
bool lastSystemState = isSystemPaused ;
bool isFinished = false ;
unsigned long runStartMSecs = 0 ;
unsigned long totalRunMSecs = 12 ;
bool isWinding = false ;
void loop()
{
	bool isOK ;
  isOK = winder_isProcessingCommands() ;
	Screen_loop() ;
#ifdef USE_ENDSTOPS
  if(lastSystemState != isSystemPaused)
  {
    if(isSystemPaused)
    {
      Serial.println("system paused") ;
    }
    else
    {
      Serial.println("system resumed") ;      
    }
    lastSystemState = isSystemPaused ;
  }
  /*if(digitalRead(9) > 0) // X-Endstop
  {
    stepper_pause() ;
  }
  else
  if(digitalRead(10) > 0) // Y-Endstop
  {
    stepper_resume() ;
  }*/
#endif
  if(isWinding)
  {
    if(!isFinished)
    {
      if((millis() - runStartMSecs) > totalRunMSecs)
      {
        isFinished = true ;  
        lcd.clear() ;
        lcd.setCursor(0,0) ;
        lcd.print("Finished") ;
      }
    }
    else
    {
      return ;
    }
  }
 //Serial.println(millis()) ;
  if(!stepper_canAddLine())
  {
    uint32_t cmillis = micros() ;
    if(cmillis - lastReportPosMillis >= 1000000)
    {
      uint32_t maxVal = 0 ;
      uint32_t pos = stepper_reportCurrentPos(&maxVal) ;
      Serial.print("pos: ") ;
      Serial.print(pos) ;
      Serial.print("/") ;
      Serial.println(maxVal) ;
      lastReportPosMillis = cmillis ;
      /*uint32_t ccv = tcounter ;
      Serial.print(cmillis - lastReportPosMillis) ;
      Serial.print(": ") ;
      Serial.println((ccv - oldCounterValue)) ; 
      oldCounterValue = ccv ;
      lastReportPosMillis = cmillis ;*/
    }
  }
  
/*  if(isOK != winder_isProcessingCommands())
  {
    Serial.println("Command added") ;
  }*/
	if(processQueueCommands())
	{
#ifdef COMMANDS_FROM_SERIAL
		// read more lines 
		// 
		isOK = true ;
		if(freeComm == 0)
		{
			freeComm = commandBuff_allocEntry() ;
		}
		if(freeComm != 0)
		{
			if(_readline.loop_nonBlock(&isOK))
			{
				if(isOK)
				{
					ParseResult pr = parseCommand(_readline.line(), freeComm) ;
					if(pr != PE_NoError)
					{
						Serial.print("COMM-Err: Parse error: ") ;
						Serial.println((int)pr) ;
					}
					else{
						Serial.println("COMM-OK") ;
					}
				} // if isOK 
				else 
				{
					Serial.println("COMM-Err: Command too long") ;
				}
			} // if readline 
		} // if freeCom 
#endif    
	} // if processQueueCommands

  //unsigned long ct = micros() ;
  //stepper_run() ;
}

void systemSetRollingSpeed(float mmSec)
{
	_system_rollingSpeed = mmSec ;
	if(!stepper_canAddLine())
	{
		float interval = calcStepperRollingTimerInterval(_system_rollingSpeed) ;
		systemSetTimerInterval(interval) ;
	}
}

float systemRollingSpeed()
{
	return _system_rollingSpeed ;
}

//volatile uint16_t systemTimerCounter = 0 ;
//volatile uint16_t systemTimerThreshold = 1 ; // 100 micro second


void timerHandler()
{
  //if(systemTimerCounter == 0)
	  stepper_run() ;
  //++systemTimerCounter ;
  //systemTimerCounter %= systemTimerThreshold ;
  
  //tcounter++ ;
}

/*uint32_t getCounterValue()
{
  return tcounter ;
}*/

void systemSetTimerInterval(float timerIntervalUSec)
{
  Timer1.setPeriod(timerIntervalUSec) ;
  /*timerIntervalUSec /= 100.0f ;
  uint16_t tt = (uint16_t)timerIntervalUSec ;
  systemTimerThreshold = tt ;
  //Timer1.setPeriod(timerIntervalUSec) ;*/
}


void windoption_getStartDiam(void *p)  // float
{
	float *fp = (float*) p;
	fp[0] = mv_startDiam ;
}

bool windoption_setStartDiam(void *p)  // float 
{
	float *fp = (float*) p;
	mv_startDiam = *fp ;
	return true ;
}

void windoption_getEndDiam(void *p)  // float
{
	float *fp = (float*) p;
	fp[0] = mv_endDiam ;
}

bool windoption_setEndDiam(void *p)  // float
{
	float *fp = (float*) p;
	mv_endDiam = *fp ;
	return true ;
}

void windoption_getStartWidth(void *p)  // float
{
	float *fp = (float*) p;
	fp[0] = mv_startWidth ;
}

bool windoption_setStartWidth(void *p)  // float
{
	float *fp = (float*) p;
	mv_startWidth = *fp ;
  mv_totalWidth = *fp ;
	return true ;
}

void windoption_getRunHours(void *p)  // float
{
	float *fp = (float*) p;
	fp[0] = mv_runHours ;
}

bool windoption_setRunHours(void *p)  // float
{
	float *fp = (float*) p;
	mv_runHours = *fp ;
	return true ;
}

void windoption_getStartDir(void *p)  // int: 0-L, 1-R
{
	int *ip = (int*) p ;
	*ip = mv_isDirLeft ? 0 : 1 ;
}

bool windoption_setStartDir(void *p)  // int: 0-L, 1-R
{
	int *ip = (int*) p ;
  mv_isDirLeft = *ip == 0 ;
	//*ip = 0 ;
	return true ;
}

bool winder_startWinding() 
{
	if(!enqueueWindCommand(mv_startDiam, mv_endDiam, mv_startWidth, mv_totalWidth, mv_isDirLeft ? 0 : 1))
	{
		// show error screen
		Lcd_showErrorScreen("Queue Full") ;
		return false ;
	}
  float t = mv_runHours * 100.0f ;
  isFinished = false ;
  isWinding = true ;
  totalRunMSecs = t * 36000 ;
  runStartMSecs = millis() ;
	return true ;
}

bool winder_isProcessingCommands() 
{
	return !stepper_canAddLine() ;
}

void winder_getSpeedValue(void* p)  // float
{
	float *fp = (float*)p; 
	*fp = systemRollingSpeed() ;
}

bool winder_setSpeedValue(void *p)  // float
{
	float *fp = (float*)p;
	systemSetRollingSpeed(*fp) ;
	return true ;
}

float getYPos() 
{
	return (float)stepper_YPos() / (float) pointingStepsPerMM;
}

void winder_getGuidePos(void *p)  // int
{
	float *fp = (float*) p ;
	*fp = getYPos() + wp_change ;
}

bool winder_setGuidePos(void *p)  // int
{
	float *fp = (float*) p ;
	wp_change = *fp - getYPos() ;
	return true ;
}

int32_t abs_i(int32_t x) ;
void winder_setGuidePosFinished()
{	
	if(wp_change != 0)
	{
  Serial.print("MoveP ") ;
  Serial.println(wp_change) ;
		if(!enqueueMovePCommand(abs_i(wp_change), wp_change > 0 ? 0 : 1))
		{
			Lcd_showErrorScreen("Queue Full") ;
		}
		wp_change = 0 ;
	}
}

void winder_pauseResume() 
{
}

float winder_getTotalWindingLength() 
{
	return 0;
}

bool winder_isFilamentGuideLeft() 
{
	if(!stepper_canAddLine())
	{
		// read current line direction
		return currentDir ;
	}
	return true ;
}

float winder_getWindingSpeed() 
{
	return systemRollingSpeed() ;
}
