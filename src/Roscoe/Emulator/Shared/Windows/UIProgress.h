
#ifndef _SHARED_WINDOWS_UIPROGRESS_H_
#define _SHARED_WINDOWS_UIPROGRESS_H_

extern void UIProgressUpdate( uint64_t u64Current,
							  uint64_t u64Max,
							  void* pvPrivate );

extern void UIProgressClose( void );

extern HWND UIProgressCreate( HWND eHWND,
							  EStringID eDescriptionID );

#endif // _SHARED_WINDOWS_UIPROGRESS_H_

