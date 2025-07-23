#ifndef _rtc_h_
#define _rtc_h_

// What 0-63 in our RTC means (0=2022, 1=2023, etc...
#define	YEAR_BASELINE			2022

extern EStatus RTCGetCPUSpeed(uint32_t *pu32CPUSpeedHz);
extern uint32_t RTCGetPowerOnSeconds(void);
extern uint32_t RTCGetPowerOnHalfSeconds(void);
extern EStatus RTCInit(void);
extern void RTCSetTime(time_t eTime);
extern uint32_t RTCGetTickCounts(void);

#endif

