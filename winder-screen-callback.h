#ifndef WINDER_SCREEN_CALLBACK_H
#define WINDER_SCREEN_CALLBACK_H

void windoption_getStartDiam(void *p) ; // float
bool windoption_setStartDiam(void *p) ; // float 
void windoption_getEndDiam(void *p) ; // float
bool windoption_setEndDiam(void *p) ; // float
void windoption_getStartWidth(void *p) ; // float
bool windoption_setStartWidth(void *p) ; // float
void windoption_getRunHours(void *p) ; // float
bool windoption_setRunHours(void *p) ; // float
void windoption_getStartDir(void *p) ; // int: 0-L, 1-R
bool windoption_setStartDir(void *p) ; // int: 0-L, 1-R
bool winder_startWinding() ;

bool winder_isProcessingCommands() ;
void winder_getSpeedValue(void* p) ; // float
bool winder_setSpeedValue(void *p) ; // float
void winder_getGuidePos(void *p) ; // float
bool winder_setGuidePos(void *p) ; // float
void winder_setGuidePosFinished() ; 
void winder_pauseResume() ;

float winder_getTotalWindingLength() ;
bool winder_isFilamentGuideLeft() ;
float winder_getWindingSpeed() ;
#endif
