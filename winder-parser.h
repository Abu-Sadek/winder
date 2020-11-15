#ifndef WINDER_PARSER_H
#define WINDER_PARSER_H

#include <stdint.h>

enum Op { OP_Wind, OP_SetSpeed, OP_MovePointer, OP_Pause, OP_Resume, OP_Clear } ;

struct WindCommandData 
{
	float startDiam ;
	float targetDiam ;
	float startWidth ;
	float totalWidth ;
	float speed ;
	uint8_t direction ;
} ;

struct MovePointerCommandData
{
	float dist ;
	float speed ; // default speed 15mm/s
	uint8_t direction ;
} ;

union CommandData
{
	WindCommandData wind ;
	MovePointerCommandData movep ;
} ;

struct Command
{
	Op op ;
	
	CommandData data ; // ommand data
	
	uint8_t isFree ;
	uint8_t isInExec ;
} ;

enum ParseResult
{
	PE_NoError = 0,
	PE_NoCommand = 1,
	PE_UnexpectedParam = 2,
	PE_ExpectedNumber ,
	PE_MissingParam ,
	PE_UnsupportedCommand ,
	PE_IncompleteCommand
} ;

ParseResult parseCommand(char *s, Command *c) ;


#endif