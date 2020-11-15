#ifndef I_READLINE_H
#define I_READLINE_H

#define MAX_READLINE_LENGTH	80

#include <stdint.h>

class IByteInput
{
public:
	virtual int avail() = 0 ;
	virtual int read() = 0 ;
} ;

class ReadLine
{
	char _line[MAX_READLINE_LENGTH];
	uint8_t _bc ;
	uint8_t _pc ;
	uint8_t _rc_state ;
	
	IByteInput *_in ;
public:
	ReadLine(IByteInput *input) { _in = input ; reset() ; }
	void reset()  ;

	// returns true if end of line reached, isLineOK will be set if it doesn't exceed the max allowed length
	bool loop(bool *isLineOK) ;
	bool loop_nonBlock(bool *isLineOK) ;
	char *line() { return _line ;}
} ;

#endif