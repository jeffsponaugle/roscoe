#ifndef _UTILTASK_H_
#define _UTILTASK_H_

// Signals a UtilTask, nonblocking, with option completion callback
extern EStatus UtilTaskSignal(uint32_t u32UtilTaskKey,
							  int64_t s64MessageIn,
							  int64_t *ps64UtilTaskResponse,
							  void (*CompletionCallback)(uint32_t u32UtilTaskKey,
														 int64_t s64UtilTaskResponse));
							  
// Just like UtilTaskSignal, but blocks until it's complete
extern EStatus UtilTaskSignalBlocking(uint32_t u32UtilTasKey,
				 					  int64_t s64UtilTaskMessage,
									  int64_t *ps64UtilTaskResponse);

// Registers a task
#define	UtilTaskRegister(name, key, callback)		UtilTaskRegisterInternal(name, __FUNCTION__, key, callback)
extern EStatus UtilTaskRegisterInternal(char *peTaskName,
										const char *peRegistrantFunction,
										uint32_t u32UtilTaskKey,
										int64_t (*Task)(uint32_t u32UtilTaskKey,
													  int64_t s64Message));

extern EStatus UtilTaskInit(void);

#endif