#ifndef _SYSEVENT_H_
#define _SYSEVENT_H_

// Various types of SysEvents
#define	SYSEVENT_PRE_OS_BIT							0
#define	SYSEVENT_POST_OS_BIT						(SYSEVENT_PRE_OS_BIT + 1)
#define	SYSEVENT_FILESYSTEM_INIT_BIT				(SYSEVENT_POST_OS_BIT + 1)
#define	SYSEVENT_FILESYSTEM_SHUTDOWN_BIT			(SYSEVENT_FILESYSTEM_INIT_BIT + 1)
#define	SYSEVENT_SETTING_INIT_START_BIT				(SYSEVENT_FILESYSTEM_SHUTDOWN_BIT + 1)
#define	SYSEVENT_SETTING_INIT_END_BIT				(SYSEVENT_SETTING_INIT_START_BIT + 1)

// This always needs to be + 1 of the last SysEvent bit
#define	SYSEVENT_TYPE_COUNT							(SYSEVENT_SETTING_INIT_END_BIT + 1)

// SysEvent #defines we use elsewhere
#define	SYSEVENT_PRE_OS								(1 << SYSEVENT_PRE_OS_BIT)
#define	SYSEVENT_POST_OS							(1 << SYSEVENT_POST_OS_BIT)
#define	SYSEVENT_FILESYSTEM_INIT					(1 << SYSEVENT_FILESYSTEM_INIT_BIT)
#define	SYSEVENT_FILESYSTEM_SHUTDOWN				(1 << SYSEVENT_FILESYSTEM_SHUTDOWN_BIT)
#define	SYSEVENT_SETTING_INIT_START					(1 << SYSEVENT_SETTING_INIT_START_BIT)
#define	SYSEVENT_SETTING_INIT_END					(1 << SYSEVENT_SETTING_INIT_END_BIT)

// Bitmask for all SysEvents
#define	SYSEVENT_MASK								((1 << SYSEVENT_TYPE_COUNT) - 1)

extern EStatus SysEventInit(void);
#define	SysEventRegister(mask, callback)	SysEventRegisterInternal(mask, __FUNCTION__, callback)
extern EStatus SysEventRegisterInternal(uint32_t u32EventMask,
										const char *peRegistrantFunction,
										EStatus (*SysEventCallback)(uint32_t u32EventMask,
																	int64_t s64Message));
extern EStatus SysEventSignal(uint32_t u32EventMask,
							  int64_t s64Message);
extern EStatus SysEventAck(uint32_t u32EventMask);

#endif