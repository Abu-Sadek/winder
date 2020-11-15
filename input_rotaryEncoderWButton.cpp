#include <Arduino.h>

#include "input_rotaryEncoderWButton.h"

#define LIBCALL_PINCHANGEINT

#include "PinChangeInt/PinChangeInt.h"

#include "winder_pins.h"

  //#if defined(EN_A) && defined(EN_B)
    #define encrot0 0
    #define encrot1 2
    #define encrot2 3
    #define encrot3 1
  //#endif
  
#define PENDING(NOW,SOON) ((long)(NOW-(SOON))<0)
#define ELAPSED(NOW,SOON) (!PENDING(NOW,SOON))

static volatile int input_buttonDiff = 0 ;

static volatile int input_rotarySpinCounter = 0 ;

static unsigned long input_next_button_update_ms ;

typedef unsigned long millis_t ;

void input_buttonIsr()
{
	static uint8_t lastButtonState = 0 ;
	const millis_t now = millis();
  if (ELAPSED(now, input_next_button_update_ms)) {
	  input_next_button_update_ms = now ;
  	uint8_t buttonState = READ_ROTARY_SW ;
  	if(buttonState != lastButtonState) 
  	{
  		if(buttonState == 0)
  			input_buttonDiff++ ;
      lastButtonState = buttonState ;
  	}		
  }
}

bool input_isButtonPressed()
{
  static int buttonLastState = 0 ;
  if(input_buttonDiff != buttonLastState)
  {
    buttonLastState = input_buttonDiff ;
    return true ;
  }
  return false ;
}

void input_reset()
{
  input_rotarySpinCounter = 0 ;
  input_isButtonPressed() ;
}

int input_rotaryPosition()
{
  return input_rotarySpinCounter / 4;
}

void input_rotaryIsr()
{
  static uint8_t lastEncoderBits = 0;
  const millis_t now = millis();
	uint8_t rotary_A = READ_ROTARY_A_CLK ;
	uint8_t rotary_B = READ_ROTARY_B_DT ;
    // Manage encoder rotation
      #define ENCODER_DIFF_CW  (input_rotarySpinCounter--)
      #define ENCODER_DIFF_CCW (input_rotarySpinCounter++)
    //#endif
    #define ENCODER_SPIN(_E1, _E2) switch (lastEncoderBits) { case _E1: ENCODER_DIFF_CW; break; case _E2: ENCODER_DIFF_CCW; }

    uint8_t enc = 0;
    if (rotary_A) enc |= B01;
    if (rotary_B) enc |= B10;
    if (enc != lastEncoderBits) {
      switch (enc) {
        case encrot0: ENCODER_SPIN(encrot3, encrot1); break;
        case encrot1: ENCODER_SPIN(encrot0, encrot2); break;
        case encrot2: ENCODER_SPIN(encrot1, encrot3); break;
        case encrot3: ENCODER_SPIN(encrot2, encrot0); break;
      }
      lastEncoderBits = enc;
    }
}

void input_setup() {
  pinMode(PIN_ROTARY_A_CLK, INPUT);
  pinMode(PIN_ROTARY_B_DT, INPUT);
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);

  PCintPort::attachInterrupt(PIN_ROTARY_A_CLK, input_rotaryIsr, CHANGE);
  PCintPort::attachInterrupt(PIN_ROTARY_B_DT, input_rotaryIsr, CHANGE);
  PCintPort::attachInterrupt(PIN_ROTARY_SW, input_buttonIsr, CHANGE);
}
