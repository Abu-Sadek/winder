#include "winder-parser.h"

#include <ctype.h>

// commands:
// W D 85 H 10 T 185 F 11 L
// W: Wind layer, stackable command, params:
//	D: rolling diameter of first layer in mm
//	H: horizontal distance to end of layrer in mm
//	T: target filled diameter of spool in mm 
//	F: winding speed (extrusion speed) in mm/s
//	L/R: direction of first layer 
//
// P: move filament guide - stackable command
//	H: distance to move in mm
//	L/R: direction of movement
//
// S: set speed - non-stackable command
//	F: winding speed in mm/s
//
// H: pause - non-stackable command
// R: Resume - non-stackable command
// C: Stop current and clear stack - non-stackable command


char *skipSpaces(char *s)
{
	while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++ ;
	return s ;
}

bool readNumber(char *s, bool readSign, float &n)
{
	bool isPos = true ;
	if(readSign)
	{
		if(*s == '-' || *s == '+')
		{
			isPos = (*s == '+') ;
			++s ;
		}
	}
	
	if(!isdigit(*s))
		return false ;
	n = *s - '0' ;
	++s ;
	while(isdigit(*s))
	{
		n = n * 10 + *s - '0';
		s++ ;
	}
	float f = 0 ;
	float ff = 0.1 ;
	if(*s == '.')
	{
		++s ;
		while(isdigit(*s))
		{
			f += (float)(*s - '0') * ff ;
			s++ ;
			ff = ff / 10 ;
		}
	}
	n += f ;
	
	if(!isPos)
		n *= -1 ;
	return true ;
}

// W D 85 H 10 T 185 F 11 L
// W: Wind layer, params:
//	D: rolling diameter of first layer in mm
//	H: horizontal distance to end of layrer in mm
//	T: target filled diameter of spool in mm 
//  I: Total width after start layer 
//	F: feed rate (extrusion speed) in mm/s
//	L/R: direction of first layer 
ParseResult parseWindCommand(char *s, Command *c)
{
	c->op = OP_Wind ;
	s = skipSpaces(s) ;
	float n = 0 ;
	char cc ;
	bool isH, isF, isDi, isT, isD, isI ;
	isH = isF = isDi = isT = isD = isI = false ;
	if(*s == 0)
		return PE_IncompleteCommand ;
	while(*s != 0)
	{
		if(*(s+1) == 0)
			return PE_IncompleteCommand ;
		switch(*s)
		{
			case 'D':
			case 'H':
			case 'T':
			case 'F':
			case 'I':
				cc = *s ;
				++s ;
				s = skipSpaces(s) ;
				n = 0 ;
				if(!readNumber(s, false, n))
					return PE_ExpectedNumber ;
				if(cc == 'D')
					c->data.wind.startDiam = n , isDi = true;
				if(cc == 'H')
					c->data.wind.startWidth = n , isH = true ;
				if(cc == 'T')
					c->data.wind.targetDiam = n , isT = true ;
				if(cc == 'F')
					c->data.wind.speed = n , isF = true ;
				if(cc == 'I')
					c->data.wind.totalWidth = n, isI = true ;
				break ;
			case 'L':
			case 'R':
				if(*s == 'L')
					c->data.wind.direction = 1 ; // left
				else
					c->data.wind.direction = 0 ;
				isD = true ;
				break ;
			default:
				return PE_UnexpectedParam ;
		}
	}
	if(!isH || !isF || !isD || !isDi || !isT || !isI)
		return PE_MissingParam ;
	return PE_NoError ;
}

// P: move filament guide - stackable command
//	H: distance to move in mm
//	F: speed
//	L/R: direction of movement
ParseResult parseMovePointerCommand(char *s, Command *c)
{
	c->op = OP_MovePointer ;
	s = skipSpaces(s) ;
	float n = 0 ;
	char cc ;
	bool isH, isF, isD ;
	isH = isF = isD = false ;
	while(*s != 0)
	{
		switch(*s)
		{
			case 'H':
			case 'F':
				cc = *s ;
				++s ;
				s = skipSpaces(s) ;
				n = 0 ;
				if(!readNumber(s, false, n))
					return PE_ExpectedNumber ;
				if(cc == 'H')
					c->data.movep.dist = n , isH = true ;
				if(cc == 'F')
					c->data.movep.speed = n , isF = true ;
				break ;
			case 'L':
			case 'R':
				if(*s == 'L')
					c->data.movep.direction = 1 ; // left
				else
					c->data.movep.direction = 1 ;
				isD = true ;
				break ;
			default:
				return PE_UnexpectedParam ;
		}
	}
	if(!isH || !isF || !isD)
		return PE_MissingParam ;
	return PE_NoError ;
}

ParseResult parseCommand(char *s, Command *c)
{
	//ParseResult pe = PE_NoError ;
	while(*s != 0)
	{
		s = skipSpaces(s) ;
		if(*s == 'W')
			return parseWindCommand(s, c) ;
		
		if(*s == 'P')
			return parseMovePointerCommand(s, c) ;
		
		if(*s != 0)
			return PE_UnsupportedCommand ;
	}
	return PE_NoError ;
}
