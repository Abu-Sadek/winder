#ifndef WINDER_SCREEN_H
#define WINDER_SCREEN_H

////////////////////////////////////////////////
/////////////  Configuration part 
#define BOOTSCREEN_TIME_SECS          3
#define STATUSSCREEN_UPDATE_TIMEOUT   300
#define LCD_SIZE_COLS                 16
#define LCD_SIZE_ROWS                 2
#define LCD_MENU_EDITING_FLASH_DELAY  350
#define LCD_ERROR_SCREEN_TIME_SECS    4
////////////////////////////////////////////////

void Screen_loop() ;

class ILcd ;

void Screen_init(ILcd *lcd) ;

void Lcd_showErrorScreen(char *mess) ;


#endif
