#ifndef _EVNETLOG_H_
#define _EVENTLOG_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int  InstallEventLog();
extern int  RemoveEventLog();
extern void WriteEventLog(WORD type, const char* message); 

#ifdef __cplusplus
}
#endif

/*
	type
		EVENTLOG_ERROR_TYPE
		EVENTLOG_WARNING_TYPE
		EVENTLOG_INFORMATION_TYPE
*/


//
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: DEFAULT_EVENT_ID
//
// MessageText:
//
//  %1!s!
//
#define DEFAULT_EVENT_ID                 0x40000000L

#endif
