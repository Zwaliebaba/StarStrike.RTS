/*
 * NetProv.h
 *
 * Alex Lee, Pumpkin Studios, Nov97
 */

// ////////////////////////////////////////////////////////////////////////
// Prototypes.
extern BOOL NETsetupModem(LPVOID *addr, const char *Phoneno,UDWORD modemToUse );
extern BOOL NETsetupTCPIP(LPVOID *addr, const char *machine);
extern BOOL NETsetupIPX(LPVOID *addr);
extern BOOL NETsetupSerial(LPVOID *addr,DWORD ComPort,
									 DWORD BaudRate, 
									 DWORD StopBits,
									 DWORD Parity, 
									 DWORD FlowControl);