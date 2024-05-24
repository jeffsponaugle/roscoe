#ifndef _UILOGIN_H_
#define _UILOGIN_H_

typedef enum
{
	LOGIN_SUCCESS = 1,
	LOGIN_CLOSE = 2
} ELoginResult;

extern ELoginResult UILogin( uint64_t u64PermissionsMask );
extern void UILoginDialog( HWND hDialog );
extern void UILoginRedrawBackground( HWND hDialog );
extern void UILoginRedrawLoginText( HWND hDialog,
									WCHAR* peMessage );
extern void UILoginCloseDialog( HWND hDialog,
								ELoginResult eCloseReason );

#endif
