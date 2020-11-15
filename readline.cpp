#include "readline.h"


void ReadLine::reset()
{
	_pc = 0;
	_bc = 0;
	_line[0] = 0;
	_rc_state = 0;
}

// returns true if \r or \n is received. They are not included in the command
// set isLineOK to false if it exceeds the max command size
bool ReadLine::loop(bool *isLineOK)
{
	uint8_t c;
	while (_in->avail() > 0 || _pc != 0)
	{
		if (_pc != 0)
		{
			c = _pc;
			_pc = 0;
		}
		else
			c = _in->read();
		if (c == '\r' || c == '\n')
		{
			while (c == '\r' || c == '\n')
				c = _in->read();
			if (_bc > 0)
			{
				_pc = c;
				_line[_bc] = 0;
				_bc = 0;
				*isLineOK = true;
				return true;
			}
		}
		if (_bc == MAX_READLINE_LENGTH - 1)
		{
			// skip till end of line
			while (c != '\r' && c != '\n')
				c = _in->read();
			_bc = 0;
			_line[_bc] = 0;
			_pc = 0;
			*isLineOK = false;
			return true;
		}
		_line[_bc++] = c;
		_line[_bc] = 0;
		//std::cout << line[bc-1] ;
	}
	return false;
}

bool ReadLine::loop_nonBlock(bool *isLineOK)
{
	uint8_t c;
	//std::cout << "rc_state: " << rc_state << ", bc: " << (int)bc;
	if (_in->avail() > 0 || _pc != 0)
	{
		if (_pc != 0)
		{
			c = _pc;
			_pc = 0;
		}
		else
			c = _in->read();

		switch (_rc_state)
		{
		case 0:
			if (c == '\r' || c == '\n')
			{
				if (_bc > 0)
				{
					_pc = c;
					_line[_bc] = 0;
					_bc = 0;
					*isLineOK = true;
					return true;
				}
				break;
			}
			else
			{
				if (_bc == MAX_READLINE_LENGTH - 1)
				{
					_rc_state = 1; // line tooo long, skip till end of line
					break;
				}
				else
				{
					_line[_bc++] = c;
					_line[_bc] = 0;
				}
			}
			break;
		case 1: // line too long, skip till end of line
			if (c == '\r' || c == '\n')
			{
				_bc = 0;
				_line[0] = 0;
				*isLineOK = false;
				_rc_state = 0;
				return true;
			}
			break;
		}
	}
	return false;
}
