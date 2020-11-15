#ifndef I_SCREEN_LCD_H
#define I_SCREEN_LCD_H

#include <stdint.h>

#ifndef DEC
#define DEC 10
#endif

class ILcd
{
public:
    virtual void setCursor(uint8_t x, uint8_t y) = 0 ;
    virtual void begin(uint8_t cols, uint8_t rows) = 0 ;
    virtual void clear() = 0 ;
	virtual void backlight() = 0 ;

    virtual size_t print(const char v[]) = 0 ;
    virtual size_t print(char v) = 0 ;
    virtual size_t print(unsigned char v, int rad = DEC) = 0 ;
    virtual size_t print(int v, int rad = DEC) = 0 ;
    virtual size_t print(unsigned int v, int rad = DEC) = 0 ;
    virtual size_t print(long v, int rad = DEC) = 0 ;
    virtual size_t print(unsigned long v, int rad = DEC) = 0 ;
    virtual size_t print(double v, int rad = 2) = 0 ;
    virtual size_t print(const Printable& v) = 0 ;
} ;

#endif