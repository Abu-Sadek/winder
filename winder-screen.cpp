#include "winder-screen.h"

//#include "rotaryWButtonTest.h"
//#include "LcdTest.h"

//#include "winder-test.h"
#include <Arduino.h>

#include "winder-screen-callback.h"

#include "screen-ilcd.h"

#include "input_rotaryEncoderWButton.h"

#include "winder_pins.h"

static ILcd *_lcd = 0 ;

enum LcdScreen { LCD_BootMenu, LCD_Status, LCD_MainMenu, LCD_SetSpeed, LCD_Wind, LCD_Pause, LCD_Resume, LCD_Error };
LcdScreen _lcd_currentScreen = LCD_BootMenu;
bool _lcd_isEnter = true;
unsigned long _lcd_bootmenu_startTime;


bool Lcd_IsEnter() { return _lcd_isEnter; }

void floatToString(char *buff, double number, uint8_t digits) ;


void Lcd_setCurrentScreen(LcdScreen sid)
{
	_lcd_currentScreen = sid;
	_lcd_isEnter = true;
}

template<typename T>
T t_min(T v1, T v2)
{
	return v1 < v2 ? v1 : v2;
}


void Lcd_BootMenu_Handler()
{
	if (Lcd_IsEnter())
	{
        _lcd->clear();
        _lcd->setCursor(3, 1);
        _lcd->print("Ask Winder");
		_lcd_bootmenu_startTime = millis();
		//Serial.print("Boot screen start time:") ;
		//Serial.println(_lcd_bootmenu_startTime) ;
		return;
	}
	if ((millis() - _lcd_bootmenu_startTime) > BOOTSCREEN_TIME_SECS * 1000)
	{
		Lcd_setCurrentScreen(LCD_Status);
		//Serial.println("Closing boot screen") ;
	}
}

////////////// Status Screen


enum LCD_StatusScreen_States : byte { LCD_StatusScreen_Loop_Ready, LCD_StatusScreen_Processing_Enter, LCD_StatusScreen_Processing_Loop };
LCD_StatusScreen_States _lcd_statusScreen_state = LCD_StatusScreen_Loop_Ready;

unsigned long _statusscreen_delay;
float totalLen;
char temp[10];

void Lcd_StatusScreen_Handler()
{
	if (Lcd_IsEnter())
	{
        if (winder_isProcessingCommands())
		{
			_lcd_statusScreen_state = LCD_StatusScreen_Processing_Enter;
		}
		else
		{
            _lcd->clear();
			//01234567890123456789
			//
			//       Ready
			//////////////////////
            _lcd->setCursor(5, 1);
            _lcd->print("Ready");
			_lcd_statusScreen_state = LCD_StatusScreen_Loop_Ready;
		}
		return;
	}
	
	unsigned long now = millis() ;
  byte ndigits ;

	switch (_lcd_statusScreen_state)
	{
	case LCD_StatusScreen_Loop_Ready:
        if (winder_isProcessingCommands())
		{
			Serial.println("processing commands") ;
			_lcd_statusScreen_state = LCD_StatusScreen_Processing_Enter;
			return;
		}
		break;
	case LCD_StatusScreen_Processing_Enter:
        _lcd->clear();
        //0123456789012345
        // Run   Len:0.01m
        // Dir:L S:11 mm/s
		//////////////////////
        _lcd->setCursor(1, 0);
        _lcd->print("Run");
        _lcd->setCursor(7, 0); _lcd->print("Len:");
        _lcd->setCursor(1, 1); _lcd->print("Dir:");
        _lcd->setCursor(7, 1); _lcd->print("S:");
		_statusscreen_delay = now;
        _lcd_statusScreen_state = LCD_StatusScreen_Processing_Loop;
	case LCD_StatusScreen_Processing_Loop:
		if ((now - _statusscreen_delay) > STATUSSCREEN_UPDATE_TIMEOUT)
		{
			_statusscreen_delay = now;
            totalLen = winder_getTotalWindingLength();
			if (totalLen < 10.0f)
        ndigits = 2 ;
				//sprintf(temp, "%.2fm", (double)totalLen);
			else {
				if (totalLen < 100.0f)
					ndigits = 1 ; //sprintf(temp, "%.1fm", (double)totalLen);
				else
					ndigits = 0 ; //sprintf(temp, "%d", (int)totalLen);
			}
     floatToString(temp, totalLen, ndigits) ;
            _lcd->setCursor(11, 0); _lcd->print(temp);

            _lcd->setCursor(5, 1); _lcd->print(winder_isFilamentGuideLeft() ? "L" : "R");

            sprintf(temp, "       ");
            _lcd->setCursor(9, 1); _lcd->print(temp);
            //sprintf(temp, "%.1fmm", (double)winder_getWindingSpeed());
            floatToString(temp, winder_getWindingSpeed(), 1) ;
            _lcd->setCursor(9, 1); _lcd->print(temp);
        }
		break;
	}

	if (input_isButtonPressed())
	{
		Lcd_setCurrentScreen(LCD_MainMenu);
		//Serial.println("Nav. to Main Menu") ;
	}
}

////////////// Main Structures
enum Lcd_MenuItem_Type : byte { MenuItemType_Bool, MenuItemType_Float, MenuItemType_Int, MenuItemType_Enum, MenuItemType_Command, MenuItemType_SubMenu, MenuItemType_End, MenuItemType_Back };

struct MenuItem
{
	Lcd_MenuItem_Type type;
	const char * title;
	void(*preRender)(MenuItem *);
	void(*getValue)(void*);
	bool(*setValue)(void*);
	bool(*actionHandler)(MenuItem*); // returns true to exit menu, false to stay showing menu
	float maxValue, minValue;
	float step;
    const char **enumVals;
	byte valueLen;
	byte viewRow;
};

struct Menu
{
	byte firstViewItem;
	byte currentItem;
	byte isInSubMenu;
	byte count;
	byte prerendered;
	byte painted;
	//  long rotaryCounter ;

	MenuItem *menuItems;
	Menu *parent;
};

static char viewBuff[LCD_SIZE_COLS + 1];

void clearViewBuff()
{
    for(int i=0;i<LCD_SIZE_COLS;++i)
        viewBuff[i] = ' ';
}

static unsigned long menu_enterTime;
static unsigned long menu_flashTime;
static byte menu_flash_isShow = 1;
static int menu_rotaryCounter;

static Menu *menu_current;
enum MenuState : byte { MenuState_Navigate, MenuState_ButtonClicked, MenuState_Editing };
static MenuState menu_state = MenuState_Navigate;

byte menu_countItems(MenuItem *items)
{
	byte c = 0;
	while (items[c].type != MenuItemType_End && c < 255)
		c++;
	return c;
}

byte intToString(char *buff, int32_t n)
{
  byte l = 0;
  if (n < 0)
  {
    *buff++ = '-';
    n = -n;
    ++l;
  }
  if (n == 0)
  {
    *buff = '0';
    ++l;
    return l;
  }
  byte len = 0;
  int32_t on = n;
  while (n > 0)
  {
    len++;
    n = n / 10;
    ++l;
  }
  n = on;
  buff[len] = 0;
  while (n > 0)
  {
    buff[len - 1] = n % 10 + '0';
    n = n / 10;
    len--;
  }
  return l;
}

void floatToString(char *buff, double number, uint8_t digits)
{
  // Handle negative numbers
  char *obuff = buff ;
  if (number < 0.0)
  {
    *buff++ = '-';
    number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i = 0; i < digits; ++i)
    rounding /= 10.0;

  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  if (int_part == 0)
    *buff++ = '0';
  else
    buff += intToString(buff, int_part);

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0)
  {
    *buff++ = '.';

    // Extract digits from the remainder one at a time
    while (digits-- > 0)
    {
      remainder *= 10.0;
      byte toPrint = int(remainder) ;
      *buff++ = toPrint + '0';
      remainder -= toPrint;
    }
  }
  *buff = 0;
}


void menu_renderMenuItemValue(MenuItem *p, int row)
{
	bool bVal;
	int iVal;
	float fVal;
  byte ndigits ;

	const char *fTempl;

    if(p->type == MenuItemType_SubMenu)
    {
        _lcd->setCursor(LCD_SIZE_COLS - 1, row);
        _lcd->print(">");
        return;
    }

    if (!p->getValue)
		return;

	switch (p->type)
	{
	case MenuItemType_Bool:
		(*(p->getValue))(&bVal);
		sprintf(temp, "%s", bVal ? "ON" : "OFF");
		break;

	case MenuItemType_Enum:
		if (!p->enumVals)
			return;
		(*(p->getValue))(&iVal);
        strcpy(viewBuff, p->enumVals[iVal]);
		break;

	case MenuItemType_Float:
		(*p->getValue)(&fVal);
		if (fVal < 10.0f)
			ndigits = 2 ; //fTempl = "%.2f";
		else
		{
			if (fVal < 100.0f)
				ndigits = 1 ; // fTempl = (char*)"%.1f";
			else
				ndigits = 0 ; //fTempl = "%.0f";
		}
		floatToString(viewBuff, fVal, ndigits) ;
		//sprintf(viewBuff, fTempl, fVal);
		break;

	case MenuItemType_Int:
		(*(p->getValue))(&iVal);
		sprintf(viewBuff, "%d", iVal);
		break;

	case MenuItemType_SubMenu:
        _lcd->setCursor(LCD_SIZE_COLS - 1, row);
        _lcd->print(">");
		return;

	default:
		return;
	}

	p->valueLen = strlen(viewBuff);
    _lcd->setCursor(LCD_SIZE_COLS - p->valueLen, row);
    _lcd->print(viewBuff);
}

void menu_setCurrent(Menu *m, bool isBack = false)
{
	menu_current = m;
    menu_rotaryCounter = input_rotaryPosition() ;
	menu_enterTime = millis();
	menu_state = MenuState_Navigate;
	if (!isBack)
	{
		if (m->count == 0)
			m->count = menu_countItems(m->menuItems);
		m->firstViewItem = 0;
		m->currentItem = 0;
	}
    m->painted = 0 ;
}

void menu_exit();

void menu_navigateHandler(Menu* menu)
{
    int pos = input_rotaryPosition() ;
	if (pos != menu_rotaryCounter)
	{
		if (pos > menu_rotaryCounter) // next item 
		{
			if (menu->currentItem < menu->count - 1)
			{
				menu->currentItem++;
				menu->painted = 0;
                if (menu->firstViewItem < menu->currentItem - LCD_SIZE_ROWS + 1)
                    menu->firstViewItem = menu->currentItem - LCD_SIZE_ROWS + 1;
                if (menu->firstViewItem >= menu->currentItem + LCD_SIZE_ROWS)
					menu->firstViewItem = menu->currentItem;
			}
		} // if next item
		else if (pos < menu_rotaryCounter)
		{
			if (menu->currentItem > 0)
			{
				menu->currentItem--;
				menu->painted = 0;
				if (menu->firstViewItem > menu->currentItem || menu->firstViewItem + LCD_SIZE_ROWS - 1 < menu->currentItem)
					menu->firstViewItem = menu->currentItem;
			}
		}
		menu_rotaryCounter = pos;
	}
	if (input_isButtonPressed() && millis() - menu_enterTime > 500)
		menu_state = MenuState_ButtonClicked;
} // menu_navigateHandler

void menu_buttonClickedHandler(Menu *menu)
{
	auto t = &menu->menuItems[menu->currentItem];
	if (t->type == MenuItemType_SubMenu || t->type == MenuItemType_Command)
	{
		bool r = (*t->actionHandler)(t);
		if (r) // exit all menus
		{
			menu_exit();
		}
	}
	if (t->type == MenuItemType_Back)
	{
		if (menu->parent == 0)
		{
			menu_exit();
			return;
		}
		menu_setCurrent(menu->parent, true);
	}
	if (t->type == MenuItemType_Bool || t->type == MenuItemType_Enum || t->type == MenuItemType_Int || t->type == MenuItemType_Float)
		menu_state = MenuState_Editing;
} // menu_buttonClickedHandler

unsigned long _calcStep_lastMillis = 0 ;
float _calcStep_lastCount = 1.0f ;
float calcStep(float step)
{
  unsigned long mills = millis() ;
  if(mills - _calcStep_lastMillis > 300)
  {
    _calcStep_lastCount = 1.0f ;
    _calcStep_lastMillis = mills ;
    return step ;
  }
  _calcStep_lastCount += 1.0f ;
  return step * _calcStep_lastCount ;
}

void menu_editHandler(Menu *menu)
{
	int iVal;
	float fVal;
	bool bVal;
	auto cmi = &menu->menuItems[menu->currentItem];
	if (!cmi->getValue || !cmi->setValue || input_isButtonPressed())
	{
		menu_state = MenuState_Navigate;
		menu_renderMenuItemValue(cmi, cmi->viewRow);
		if(cmi->actionHandler)
			(*cmi->actionHandler)(cmi) ;
		menu->painted = 0;
		return;
	}
	if (millis() - menu_flashTime > LCD_MENU_EDITING_FLASH_DELAY)
	{
        menu_flashTime = millis() ;
		menu_flash_isShow = menu_flash_isShow ? 0 : 1;
		if (menu_flash_isShow)
		{
			menu_renderMenuItemValue(cmi, cmi->viewRow);
		}
		else {
			for (int i = 0; i < cmi->valueLen; ++i)
			{
                _lcd->setCursor(LCD_SIZE_COLS - cmi->valueLen + i, cmi->viewRow);
                _lcd->print(" ");
			}
		} // else
	}
    int rp = input_rotaryPosition() ;
    //int drp = abs(rp - menu_rotaryCounter) ;
	if (menu_rotaryCounter != rp)
	{
        if (rp > menu_rotaryCounter)
		{
			switch (cmi->type)
			{
			case MenuItemType_Bool:
				bVal = false;
				(*cmi->setValue)(&bVal);
				break;

			case MenuItemType_Enum:
				(*cmi->getValue)(&iVal);
				if (iVal > 0)
					iVal--;
				(*cmi->setValue)(&iVal);
				break;

			case MenuItemType_Int:
				(*cmi->getValue)(&iVal);
				if (iVal > cmi->minValue)
					iVal -= calcStep(cmi->step);
         if(iVal < cmi->minValue)
          iVal = cmi->minValue ;
				(*cmi->setValue)(&iVal);
				break;

			case MenuItemType_Float:
				(*cmi->getValue)(&fVal);
				if (fVal > cmi->minValue)
					fVal -= calcStep(cmi->step);
                if(fVal < cmi->minValue)
                    fVal = cmi->minValue ;
				(*cmi->setValue)(&fVal);
				break;
			} // switch
		} // if move down
		else // move up
		{
			switch (cmi->type)
			{
			case MenuItemType_Bool:
				bVal = true;
				(*cmi->setValue)(&bVal);
				break;

			case MenuItemType_Enum:
				(*cmi->getValue)(&iVal);
				if (iVal < cmi->maxValue)
					iVal++;
				(*cmi->setValue)(&iVal);
				break;

			case MenuItemType_Int:
				(*cmi->getValue)(&iVal);
				if (iVal < cmi->maxValue)
				{
					iVal += calcStep(cmi->step);
					if (iVal > cmi->maxValue)
						iVal = cmi->maxValue;
					(*cmi->setValue)(&iVal);
				}
				break;

			case MenuItemType_Float:
				(*cmi->getValue)(&fVal);
				if (fVal < cmi->maxValue)
				{
					fVal += calcStep(cmi->step);
					if (fVal > cmi->maxValue)
						fVal = cmi->maxValue;
					(*cmi->setValue)(&fVal);
				}
				break;
			} // switch
		} // else 
        menu_rotaryCounter = rp ;
	} // rotary moved
}

bool menu_handler()
{
	Menu* menu = menu_current;

	if (!menu->painted)
	{
        _lcd->clear() ;
		// loop over viewable items and call prerender if exists
		for (int i = menu->firstViewItem; i < menu->count && i < menu->firstViewItem + LCD_SIZE_ROWS; ++i)
		{
			if (menu->menuItems[i].preRender)
				menu->menuItems[i].preRender(&menu->menuItems[i]);
		}

		for (int i = menu->firstViewItem; i < menu->count && i < menu->firstViewItem + LCD_SIZE_ROWS; ++i)
		{
			if (menu->menuItems[i].title)
			{
                MenuItem *cmi = &menu->menuItems[i];
				int l = strlen(cmi->title);
                byte sl = min(l, LCD_SIZE_COLS - 1);
                clearViewBuff();
                strncpy(&viewBuff[1], cmi->title, sl);
                viewBuff[sl+1] = '\0' ;
				viewBuff[0] = (menu->currentItem == i) ? '>' : ' ';
				viewBuff[LCD_SIZE_COLS] = '\0';
				cmi->viewRow = i - menu->firstViewItem;
                _lcd->setCursor(0, cmi->viewRow);
                _lcd->print(viewBuff);
				menu_renderMenuItemValue(cmi, cmi->viewRow);
			}
		} // for render 
		menu->painted = 1;
	} // if ! painted

	switch (menu_state)
	{
	case MenuState_Navigate:
		menu_navigateHandler(menu);
		if (menu_state != MenuState_ButtonClicked)
			break;

	case MenuState_ButtonClicked:
		menu_buttonClickedHandler(menu);
		menu_flashTime = millis();
		menu_flash_isShow = 1;
        menu_rotaryCounter = input_rotaryPosition() ;
		break;

	case MenuState_Editing:
		menu_editHandler(menu);
		break;
	}
    return true ;
}


////////////  Pause/Resume Menu Item 

bool isPaused = false;

void pauseResumePreRender(MenuItem *p)
{
	if (isPaused)
		p->title = "Resume";
	else
		p->title = "Pause";
}

bool pauseResumeHandler(MenuItem *p) // isEnter is negligible as it is not a menu
{
    winder_pauseResume() ;
	isPaused = !isPaused;
	return true;
}

//////////// Error Screen Handler
uint32_t _lcd_ErrorScreen_time ;
char *_lcd_ErrorScreen_mess ;
void Lcd_ErrorScreen_Handler()
{	
	if(Lcd_IsEnter())
	{
		_lcd->clear() ;
		_lcd->setCursor(4, 0) ;
		_lcd->print("Error") ;
		int l = _lcd_ErrorScreen_mess == 0 ? 0 : strlen(_lcd_ErrorScreen_mess) ;
		if(l > 0)
		{
			_lcd->setCursor((LCD_SIZE_COLS - l)/2, 1) ;
			_lcd->print(_lcd_ErrorScreen_mess) ;
		}
		_lcd_ErrorScreen_time = millis() ;
	}
	if((millis() - _lcd_ErrorScreen_time) / 1000 > LCD_ERROR_SCREEN_TIME_SECS)
	{
		Lcd_setCurrentScreen(LCD_Status) ;
	}
}

void Lcd_showErrorScreen(char *mess)
{
	_lcd_ErrorScreen_mess = mess ;
	Lcd_setCurrentScreen(LCD_Status) ;
}

//////////// Main Menu

bool startWindMenu(MenuItem *p);

const char * enumDirVals[] = { "L", "R" };

bool windmenu_startWinding(MenuItem *p)
{
    return winder_startWinding();
}

MenuItem windMenuItems[] = {
  {.type = MenuItemType_Back,.title = "<-",.preRender = 0,.getValue = 0,.setValue = 0,.actionHandler = 0,.maxValue = 0,.minValue = 0,.step = 0}
  ,{.type = MenuItemType_Float,.title = "Start Diam",.preRender = 0,.getValue = windoption_getStartDiam,.setValue = windoption_setStartDiam,.actionHandler = 0,.maxValue = 400,.minValue = 40,.step = 1}
  ,{.type = MenuItemType_Float,.title = "End Diam",.preRender = 0,.getValue = windoption_getEndDiam,.setValue = windoption_setEndDiam,.actionHandler = 0,.maxValue = 400,.minValue = 40,.step = 1}
  ,{.type = MenuItemType_Float,.title = "Spool Width",.preRender = 0,.getValue = windoption_getStartWidth,.setValue = windoption_setStartWidth,.actionHandler = 0,.maxValue = 200,.minValue = 1.75,.step = 1.75f}
  ,{.type = MenuItemType_Float,.title = "Run Hours",.preRender = 0,.getValue = windoption_getRunHours,.setValue = windoption_setRunHours,.actionHandler = 0,.maxValue = 12,.minValue = 0.05,.step = 0.1f}
  ,{.type = MenuItemType_Enum,.title = "Start Dir",.preRender = 0,.getValue = windoption_getStartDir,.setValue = windoption_setStartDir,.actionHandler = 0,.maxValue = 1,.minValue = 0,.step = 1,.enumVals = enumDirVals}
  ,{.type = MenuItemType_Command,.title = "Start Winding",.preRender = 0,.getValue = 0,.setValue = 0,.actionHandler = windmenu_startWinding,.maxValue = 0,.minValue = 0,.step = 0}
  ,{.type = MenuItemType_End,.title = 0,.preRender = 0,.getValue = 0,.setValue = 0,.actionHandler = 0,.maxValue = 0,.minValue = 0,.step = 0}
};

bool winder_setGuidePosFinished_handler(MenuItem *) ;

MenuItem mainMenuItems[] = {
  {.type = MenuItemType_Back,.title = "<-",.preRender = 0,.getValue = 0,.setValue = 0,.actionHandler = 0,.maxValue = 0,.minValue = 0,.step = 0}
  ,{.type = MenuItemType_Float,.title = "Set speed",.preRender = 0,.getValue = winder_getSpeedValue,.setValue = winder_setSpeedValue,.actionHandler = 0,.maxValue = 260,.minValue = 1,.step = 0.1}
  ,{.type = MenuItemType_Float,.title = "Move Guide",.preRender = 0,.getValue = winder_getGuidePos,.setValue = winder_setGuidePos,.actionHandler = winder_setGuidePosFinished_handler,.maxValue = 1000,.minValue = -1000,.step = 0.25}
  ,{.type = MenuItemType_SubMenu,.title = "Wind",.preRender = 0,.getValue = 0,.setValue = 0,.actionHandler = startWindMenu,.maxValue = 0,.minValue = 0,.step = 0}
  ,{.type = MenuItemType_Command,.title = "Pause",.preRender = pauseResumePreRender,.getValue = 0,.setValue = 0,.actionHandler = pauseResumeHandler,.maxValue = 0,.minValue = 0,.step = 0}
  ,{.type = MenuItemType_End,.title = 0,.preRender = 0,.getValue = 0,.setValue = 0,.actionHandler = 0,.maxValue = 0,.minValue = 0,.step = 0}
};

Menu mainMenu[] = { {.firstViewItem = 0 ,.currentItem = 0,.isInSubMenu = 0,.count = 0,.prerendered = 0,.painted = 0, /*.rotaryCounter = 0,*/ .menuItems = mainMenuItems } };

Menu windMenu[] = { {.firstViewItem = 0 ,.currentItem = 0,.isInSubMenu = 0,.count = 0,.prerendered = 0,.painted = 0, /*.rotaryCounter = 0,*/ .menuItems = windMenuItems,.parent = &mainMenu[0] } };

bool startWindMenu(MenuItem *p)
{
	menu_setCurrent(windMenu);
	return false; // if returned true, the menu will be cancelled
}

bool winder_setGuidePosFinished_handler(MenuItem *)
{
	winder_setGuidePosFinished() ;
	return true ;
}

////////////// LCD main functions

void Screen_init(ILcd *lcd)
{
    _lcd = lcd ;
	Lcd_setCurrentScreen(LCD_BootMenu);
}

/*extern volatile int input_buttonDiff = 0 ;

extern volatile int input_rotarySpinCounter = 0 ;

int lastButtonDiff = 0;
void dump_input()
{
  //Serial.print("R-C: ") ; Serial.print((int)READ_ROTARY_SW) ;
  if(input_buttonDiff != lastButtonDiff)
  {
	  Serial.print("R-C: ") ; Serial.println((int)input_buttonDiff) ;
	  lastButtonDiff = input_buttonDiff ;
  }
  //Serial.print("R-A: ") ; Serial.print((int)READ_ROTARY_A_CLK) ;
  //Serial.print("R-B: ") ; Serial.println((int)READ_ROTARY_B_DT) ;
}*/

int rp = 0 ;
void Screen_loop()
{
	LcdScreen ps = _lcd_currentScreen;
	switch (_lcd_currentScreen)
	{
	case LCD_BootMenu:
		Lcd_BootMenu_Handler();
		break;
	case LCD_Status:
		Lcd_StatusScreen_Handler();
		break;
	case LCD_MainMenu:
		if (_lcd_isEnter)
			menu_setCurrent(mainMenu, false);
		menu_handler();
		break;
	case LCD_Error:
		Lcd_ErrorScreen_Handler() ;
		break ;
	}

	if (ps == _lcd_currentScreen)
		_lcd_isEnter = false;
	else
	{
		_lcd_isEnter = true;
		//Serial.println("switching screen") ;
	}
}

void menu_exit()
{
    _lcd_isEnter = true;
    Lcd_setCurrentScreen(LCD_Status);
}
