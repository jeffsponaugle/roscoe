
#ifndef _SHARED_WINDOWS_UIUPDATE_H_
#define _SHARED_WINDOWS_UIUPDATE_H_

typedef void (*UIUpdateShutdownCallback)(void);

extern void UIProgressUpdate( uint64_t u64Current,
							  uint64_t u64Max,
							  void* pvPrivate );

extern void UIUpdateRegisterShutdownCallback( UIUpdateShutdownCallback pfCallback );

extern void UIUpdateCheckForUpdate( HWND eHWND );

#endif // _SHARED_WINDOWS_UIUPDATE_H_
