#include <stdarg.h>
#include "Shared/Shared.h"
#include "Arch/Arch.h"
#include "OS/OSPort.h"
#include "Platform/Platform.h"
#include "Shared/TwoFish/AES.h"

// Allowed filename characters that are not part of directory designators
static const char *sg_peFilenameChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_!@#$%^&()-=_+][{}|';:\",<>";

// Size of cryptographic buffer
#define	CRYPT_BUFFER_SIZE				512

// File signature
#define	FILE_SIGNATURE					"FILE"

// Size of buffer during file copy
#define		FILE_COPY_BUFFER_SIZE		1024

// Encrypted file
typedef struct SFileCrypt
{
	char *peCryptBuffer;				// Buffer for encryption/decryption so it's passive to the caller
	keyInstance sKeyInstance;
	cipherInstance sCipherInstance;
	bool bRead;							// Set true if we're reading
} SFileCrypt;

// Standard file
typedef struct SFileInternal
{
	char eFileSignature[4];				// Signature for pointer validity check
	SOSFile sFile;						// Port file pointer
	SFileCrypt *psCrypt;				// If this is an encrypted file, we have an SFileCrypt structure
} SFileInternal;

// Secret key used for all files
static uint8_t sg_u8FilePrivateKey[] = 
{
	0x44, 0xfa, 0x37, 0xc5, 0xd2, 0x96, 0x3e, 0xbb, 0xde, 0x72, 0x70, 0x00, 0x17, 0x03, 0xdf, 0x47,
	0x18, 0xd7, 0x04, 0x45, 0x1f, 0x80, 0x07, 0x63, 0x77, 0x23, 0x6d, 0x90, 0x9d, 0x84, 0x53, 0x81
};

// Checks file pointer for validity - both address-wise and content-wise
EStatus FileIsValid(SOSFile sFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) sFile;

	// NULL Pointers are never valid
	if (NULL == psFileInternal)
	{
		return(ESTATUS_FILE_HANDLE_INVALID);
	}

	// Check to ensure the pointer itself is valid first
	if (false == IsPointerValid((void *) psFileInternal,
								false))
	{
		return(ESTATUS_FILE_HANDLE_INVALID);
	}

	// Let's see if it has our file signature
	if (memcmp((void *) psFileInternal->eFileSignature, (void *) FILE_SIGNATURE, sizeof(psFileInternal->eFileSignature)))
	{
		return(ESTATUS_FILE_HANDLE_INVALID);
	}

	// Appears to be an actual file handle
	return(ESTATUS_OK);
}

// If the incoming filename contains / or . before any alphanumeric characters,
// the peFilename will be used as-is and the platform's base pathname will be
// ignored.
//
// If the platform does not have the concept of an absolute pathname and returns
// NULL, the filename will be used as-is.
//
// If the platform has a base path name and there are no / or . before any alphanumeric
// characters, it will become the prefix to *ppeAllocatedFilename.

static EStatus FileConstructAbsoluteFilename(char *peFilename,
											 char **ppeAllocatedFilename)
{
	EStatus eStatus;
	char *peBasePathname;
	char *peFileChar;
	bool bDirSep = false;
	bool bAlpha = false;

	// Get the base pathname
	peBasePathname = PlatformGetBasePathname();
	
	// If we don't have a base pathname, then just ignore it
	if (NULL == peBasePathname)
	{
		*ppeAllocatedFilename = NULL;
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// We have a base pathname. Let's go through the filename and see if we see . or /
	// before we see an alphanumeric character
	peFileChar = peFilename;
	while (*peFileChar)
	{
		// If it's a '.' or '/', then we need to exit immediately
		if (('.' == *peFileChar) ||
			('/' == *peFileChar))
		{
			bDirSep = true;
			break;
		}

		// Is it alpha/special?
		if (strchr(sg_peFilenameChars, *peFileChar))
		{
			bAlpha = true;
			break;
		}

		++peFileChar;
	}

	// If we've encountered a directory separator first, then we just return, because
	// the incoming filename is absolute rather than relative
	if (bDirSep)
	{
		*ppeAllocatedFilename = NULL;
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// If bAlpha is false, then it means no alpha characters and no directory separators,
	// which likely means it's a filename with nothing in it, or it's full of characters
	// that aren't present in sg_peFilenameChars.
	if (false == bAlpha)
	{
		*ppeAllocatedFilename = NULL;
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// We know now that the incoming filename will need to be appended to the platform
	// prefix.
	*ppeAllocatedFilename = MemAllocNoClear((uint32_t) (strlen(peFilename) + strlen(peBasePathname) + 1));
	if (NULL == *ppeAllocatedFilename)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	// Copy in the base pathname
	strcpy(*ppeAllocatedFilename, peBasePathname);
	strcat(*ppeAllocatedFilename, peFilename);

	// All good
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

EStatus FilesystemInit(uint32_t u32Mask,
					   int64_t s64Message)
{
	EStatus eStatus;
	uint32_t u32UniqueIDLength;

	u32UniqueIDLength = sizeof(sg_u8FilePrivateKey);
	eStatus = PlatformGetUniqueSystemID(sg_u8FilePrivateKey,
										&u32UniqueIDLength);
	ERR_GOTO();

	eStatus = PortFilesystemInit();

errorExit:
	return(eStatus);
}

EStatus FilesystemMount(void)
{
	return(PortFilesystemMount());
}

void FilesystemRemount(bool bWaitForMount)
{
	PortFilesystemRemount(bWaitForMount);
}

EFilesystemType FilesystemGetType(void)
{
	return(PortFilesystemGetType());
}

static EStatus FileInternalDeallocate(SFileInternal *psFileInternal)
{
	EStatus eStatus = ESTATUS_OK;
	
	if (psFileInternal)
	{
		// Get rid of the signature so it will be detected as invalid if referenced
		memset((void *) psFileInternal->eFileSignature, 0, sizeof(psFileInternal->eFileSignature));

		// Close the file (in case it got opened)
		eStatus = PortFilefclose(psFileInternal->sFile);
		
		if (psFileInternal->psCrypt)
		{
			if (psFileInternal->psCrypt->peCryptBuffer)
			{
				MemFree(psFileInternal->psCrypt->peCryptBuffer);
			}
			
			MemFree(psFileInternal->psCrypt);
		}
		
		MemFree(psFileInternal);
	}
	
	return(eStatus);
}

static EStatus FileInternalCreate(SFileInternal **ppsFileInternal,
								  char *peFileMode,
								  SOSFile psFile,
								  bool bEncrypted)
{
	EStatus eStatus;
	SFileInternal *psFileInternal = NULL;
	
	psFileInternal = MemAlloc(sizeof(*psFileInternal));
	if (NULL == psFileInternal)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}
	
	// Copy in the file pointer
	memcpy((void *) &psFileInternal->sFile, (void *) &psFile, sizeof(psFileInternal->sFile));

	// Copy in the file signature
	memcpy((void *) psFileInternal->eFileSignature, (void *) FILE_SIGNATURE, sizeof(psFileInternal->eFileSignature));

	// Are we encrypted? If so, create an encryption structure
	if (bEncrypted)
	{
		BYTE eDirection = DIR_DECRYPT;
		
		psFileInternal->psCrypt = MemAlloc(sizeof(*psFileInternal->psCrypt));
		if (NULL == psFileInternal->psCrypt)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}
		
		if (strcmp(peFileMode, "rb") == 0)
		{
			// Read
			psFileInternal->psCrypt->bRead = true;
		}
		else
		if (strcmp(peFileMode, "wb") == 0)
		{
			// Write
			eDirection = DIR_ENCRYPT;
		}
		else
		{
			eStatus = ESTATUS_INVALID_FILE_MODE;
			goto errorExit;
		}

		// Make key
		if (makeKey(&psFileInternal->psCrypt->sKeyInstance, eDirection, sizeof(sg_u8FilePrivateKey) << 3, NULL) != true)
		{
			eStatus = ESTATUS_CIPHER_INIT_FAILURE;
			goto errorExit;
		}
		
		// Cipher init
		if (cipherInit(&psFileInternal->psCrypt->sCipherInstance, MODE_CFB1, NULL) != true)
		{
			DebugOut("Failed makeKey\r\n");
			eStatus = ESTATUS_CIPHER_INIT_FAILURE;
			goto errorExit;
		}
		
		// If this asserts, it means our security key is bigger than TwoFish needs
		BASSERT(sizeof(sg_u8FilePrivateKey) == sizeof(psFileInternal->psCrypt->sKeyInstance.key32));

		// Copy in our key data
		memcpy((void *) psFileInternal->psCrypt->sKeyInstance.key32, (void *) sg_u8FilePrivateKey, sizeof(psFileInternal->psCrypt->sKeyInstance.key32));
		
		// Rekey it with our security key
		reKey(&psFileInternal->psCrypt->sKeyInstance);		
		
		// Create crypto buffer
		psFileInternal->psCrypt->peCryptBuffer = MemAllocNoClear(CRYPT_BUFFER_SIZE);
		if (NULL == psFileInternal->psCrypt->peCryptBuffer)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}
	}
	
	*ppsFileInternal = psFileInternal;
	eStatus = ESTATUS_OK;
	
errorExit:
	if (eStatus != ESTATUS_OK)
	{
		(void) FileInternalDeallocate(psFileInternal);
	}
	
	return(eStatus);
}

static EStatus FilefopenInternal(SOSFile *psFile, 
								char *peFilename, 
								char *peFileMode,
								bool bEncrypted)
{
	EStatus eStatus = ESTATUS_OK;
	char *peTempFilename = NULL;
	SOSFile sFile;
	
/*	eStatus = FileConstructAbsoluteFilename(peFilename,
											&peTempFilename);
	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	} */

	if (peTempFilename)
	{
		// This means we had to construct it
		eStatus = PortFilefopen(&sFile,
							   peTempFilename, 
							   peFileMode);

		MemFree(peTempFilename);
	}
	else
	{
		// This means we use the filename as-is
		eStatus = PortFilefopen(&sFile,
							   peFilename,
							   peFileMode);
	}
	
	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}

	eStatus = FileInternalCreate((SFileInternal **) psFile,
								 peFileMode,
								 sFile,
								 bEncrypted);

errorExit:
	return(eStatus);
}

EStatus Filefopen(SOSFile *psFile, char *peFilename, char *peFileMode)
{
	return(FilefopenInternal(psFile,
							 peFilename,
							 peFileMode,
							 false));
}

EStatus FileOpenEncrypted(SOSFile *psFile, char *peFilename, char *peFileMode)
{
	return(FilefopenInternal(psFile,
							peFilename,
							peFileMode,
							true));
}

static EStatus FileOpenUniqueFileInternal(char *pePath, 
										  char *peFilenameGenerated, 
										  uint32_t u32FilenameBufferSize, 
										  SOSFile *psOpenedFile,
										  bool bEncrypted)
{
	EStatus eStatus = ESTATUS_OK;
	char *peTempPath = NULL;
	SOSFile sOpenedFile;

	memset((void *) &sOpenedFile, 0, sizeof(sOpenedFile));
	
	eStatus = FileConstructAbsoluteFilename(pePath,
											&peTempPath);
	ERR_GOTO();
	
	if (peTempPath)
	{
		eStatus = PortFileOpenUniqueFile(peTempPath,
										 peFilenameGenerated,
										 u32FilenameBufferSize,
										 &sOpenedFile);
		MemFree(peTempPath);
	}
	else
	{
		eStatus = PortFileOpenUniqueFile(pePath,
										 peFilenameGenerated,
										 u32FilenameBufferSize,
										 &sOpenedFile);
	}
	
	ERR_GOTO();
	
	eStatus = FileInternalCreate((SFileInternal **) psOpenedFile,
								 "wb",
								 sOpenedFile,
								 bEncrypted);

errorExit:
	return(eStatus);
}

EStatus FileOpenUniqueFile(char *pePath, char *peFilenameGenerated, uint32_t u32FilenameBufferSize, SOSFile *psOpenedFile)
{
	return(FileOpenUniqueFileInternal(pePath,
									  peFilenameGenerated,
									  u32FilenameBufferSize,
									  psOpenedFile,
									  false));
}

EStatus FileOpenUniqueFileEncrypted(char *pePath, char *peFilenameGenerated, uint32_t u32FilenameBufferSize, SOSFile *psOpenedFile)
{
	return(FileOpenUniqueFileInternal(pePath,
									  peFilenameGenerated,
									  u32FilenameBufferSize,
									  psOpenedFile,
									  false));
}

EStatus FileCreateUniqueDirectory(char *pePath, char *peDirectoryGenerated, uint32_t u32DirectoryBufferSize)
{
	EStatus eStatus = ESTATUS_OK;
	char *peTempPath = NULL;

	eStatus = FileConstructAbsoluteFilename(pePath,
											&peTempPath);
	ERR_GOTO();

	if (peTempPath)
	{
		eStatus = PortFileCreateUniqueDirectory(peTempPath,
												peDirectoryGenerated,
												u32DirectoryBufferSize);
		MemFree(peTempPath);
	}
	else
	{
		eStatus = PortFileCreateUniqueDirectory(pePath,
												peDirectoryGenerated,
												u32DirectoryBufferSize);
	}

errorExit:
	return(eStatus);
}

EStatus Filefread(void *pvBuffer, uint64_t u64SizeToRead, uint64_t *pu64SizeRead, SOSFile psFile)
{
	EStatus eStatus;
	SFileInternal *psFileInternal = (SFileInternal *) psFile;
	
	eStatus = FileIsValid(psFile);
	ERR_GOTO();

	if (psFileInternal->psCrypt)
	{
		uint64_t u64Chunk;
		
		// If we are writing a file, return invalid file mode
		if (false == psFileInternal->psCrypt->bRead)
		{
			return(ESTATUS_INVALID_CRYPT_MODE);
		}
		
		// If we care about how much is read, zero it out
		if (pu64SizeRead)
		{
			*pu64SizeRead = 0;
		}
		
		// We have an encrypted file. Let's read in chunks and decrypt as we go
		while (u64SizeToRead)
		{
			uint64_t u64SizeRead;
			
			u64Chunk = u64SizeToRead;
			if (u64Chunk > CRYPT_BUFFER_SIZE)
			{
				u64Chunk = CRYPT_BUFFER_SIZE;
			}

			// Read the chunk into our temporary buffer
			eStatus = PortFilefread(psFileInternal->psCrypt->peCryptBuffer,
									u64Chunk,
									&u64SizeRead,
									psFileInternal->sFile);
			if (eStatus != ESTATUS_OK)
			{
				return(eStatus);
			}
			
			// Now decrypt to target buffer
    		if (blockDecrypt(&psFileInternal->psCrypt->sCipherInstance,
							 &psFileInternal->psCrypt->sKeyInstance,
							 (BYTE *) psFileInternal->psCrypt->peCryptBuffer,
							 (int) (u64SizeRead << 3),
							 pvBuffer) != (int) (u64SizeRead << 3))
			{
				return(ESTATUS_CIPHER_DECRYPTION_FAULT);				
			}
			
			// Move pointers and decrrement counters
			pvBuffer = (void *) (((uint8_t *) pvBuffer) + u64SizeRead);
			if (pu64SizeRead)
			{
				*pu64SizeRead += u64SizeRead;
			}

			// Subtract # of bytes to read
			u64SizeToRead -= u64SizeRead;
		}
		
		return(ESTATUS_OK);
	}
	else
	{
		uint64_t u64SizeRead = 0;

		// Standard raw read from the file
		eStatus = PortFilefread(pvBuffer, 
								u64SizeToRead, 
								&u64SizeRead, 
								psFileInternal->sFile);

		if ((ESTATUS_OK == eStatus) &&
			(pu64SizeRead))
		{
			*pu64SizeRead = u64SizeRead;
		}

		return(eStatus);
	}

errorExit:
	return(eStatus);
}

EStatus Filefwrite(void *pvBuffer, uint64_t u64SizeToWrite, uint64_t *pu64SizeWritten, SOSFile psFile)
{
	EStatus eStatus;
	SFileInternal *psFileInternal = (SFileInternal *) psFile;
	
	eStatus = FileIsValid(psFile);
	ERR_GOTO();

	if (psFileInternal->psCrypt)
	{
		uint64_t u64Chunk;
		
		// If we are reading a file, return invalid file mode
		if (psFileInternal->psCrypt->bRead)
		{
			return(ESTATUS_INVALID_CRYPT_MODE);
		}
		
		// If we care about how much is written, zero it out
		if (pu64SizeWritten)
		{
			*pu64SizeWritten = 0;
		}
		
		// We have an encrypted file. Let's write in chunks and encrypt as we go
		while (u64SizeToWrite)
		{
			uint64_t u64SizeWritten;
			
			u64Chunk = u64SizeToWrite;
			if (u64Chunk > CRYPT_BUFFER_SIZE)
			{
				u64Chunk = CRYPT_BUFFER_SIZE;
			}
   					
			// Now encrypt to encryption buffer
			if (blockEncrypt(&psFileInternal->psCrypt->sCipherInstance,
							 &psFileInternal->psCrypt->sKeyInstance,
							 (BYTE *) pvBuffer,
							 (int) (u64Chunk << 3),
							 (BYTE *) psFileInternal->psCrypt->peCryptBuffer) != (u64Chunk << 3))
			{
				return(ESTATUS_CIPHER_ENCRYPTION_FAULT);				
			}
			
			// Write the chunk from our temporary (encrypted) buffer
			eStatus = PortFilefwrite(psFileInternal->psCrypt->peCryptBuffer,
									 u64Chunk,
									 &u64SizeWritten,
									 psFileInternal->sFile);
			ERR_GOTO();

			// Move pointers and decrrement counters
			pvBuffer = (void *) (((uint8_t *) pvBuffer) + u64SizeWritten);
			if (pu64SizeWritten)
			{
				*pu64SizeWritten += u64SizeWritten;
			}

			// Subtract # of bytes to write
			u64SizeToWrite -= u64SizeWritten;
		}
		
		return(ESTATUS_OK);
	}
	else
	{
		// Standard raw read from the file
		return(PortFilefwrite(pvBuffer, 
							  u64SizeToWrite, 
							  pu64SizeWritten, 
							  psFileInternal->sFile));
	}

errorExit:
	return(eStatus);
}

EStatus Filefputc(uint8_t u8Data, SOSFile sFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) sFile;

	// If this is encrypted, then encrypt it
	if (psFileInternal->psCrypt)
	{
		uint8_t u8EncryptedData;

		// If we are reading a file, return invalid file mode
		if (psFileInternal->psCrypt->bRead)
		{
			return(ESTATUS_INVALID_CRYPT_MODE);
		}

		// Now encrypt to encryption buffer
		if (blockEncrypt(&psFileInternal->psCrypt->sCipherInstance,
						 &psFileInternal->psCrypt->sKeyInstance,
						 (BYTE *) &u8Data,
						 (int) (sizeof(u8Data) << 3),
						 (BYTE *) &u8EncryptedData) != (sizeof(u8EncryptedData) << 3))
		{
			return(ESTATUS_CIPHER_ENCRYPTION_FAULT);				
		}

		return(PortFilefputc(u8EncryptedData, psFileInternal->sFile));
	}

	return(PortFilefputc(u8Data, psFileInternal->sFile));
}

EStatus Filefgetc(uint8_t *pu8Data, SOSFile sFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) sFile;

	if (psFileInternal->psCrypt)
	{
		EStatus eStatus;
		uint8_t u8Data;

		// If we are reading a file, return invalid file mode
		if (false == psFileInternal->psCrypt->bRead)
		{
			return(ESTATUS_INVALID_CRYPT_MODE);
		}

		// Read the data
		eStatus = PortFilefgetc(&u8Data, psFileInternal->sFile);
		if (eStatus != ESTATUS_OK)
		{
			return(eStatus);
		}

		// Decrypt the data
    	if (blockDecrypt(&psFileInternal->psCrypt->sCipherInstance,
						 &psFileInternal->psCrypt->sKeyInstance,
						 (BYTE *) &u8Data,
						 (int) (sizeof(u8Data) << 3),
						 pu8Data) != (int) (sizeof(*pu8Data) << 3))
		{
			return(ESTATUS_CIPHER_DECRYPTION_FAULT);				
		}

		// Decryption all good
		return(ESTATUS_OK);
	}

	return(PortFilefgetc(pu8Data, psFileInternal->sFile));
}

EStatus Filefseek(SOSFile psFile, int64_t s64Offset, int32_t s32Origin)
{
	SFileInternal *psFileInternal = (SFileInternal *) psFile;
	EStatus eStatus;

	eStatus = FileIsValid(psFile);
	ERR_GOTO();

	// Can't seek on an encrypted file
	if (psFileInternal->psCrypt)
	{
		return(ESTATUS_INVALID_CRYPT_MODE);
	}
	
	return(PortFilefseek(psFileInternal->sFile, 
						 s64Offset, 
						 s32Origin));
errorExit:
	return(eStatus);
}

EStatus Fileftell(SOSFile psFile, uint64_t *pu64Position)
{
	SFileInternal *psFileInternal = (SFileInternal *) psFile;
	EStatus eStatus;

	eStatus = FileIsValid(psFile);
	ERR_GOTO();

	return(PortFileftell(psFileInternal->sFile, 
						 pu64Position));

errorExit:
	return(eStatus);
}

EStatus Fileferror(SOSFile psFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) psFile;
	EStatus eStatus;

	eStatus = FileIsValid(psFile);
	if (eStatus != ESTATUS_OK)
	{
		return(eStatus);
	}

	return(PortFileferror(psFileInternal->sFile));
}

EStatus Filefclose(SOSFile *psFile)
{
	EStatus eStatus;

	eStatus = FileIsValid(*psFile);
	ERR_GOTO();

	eStatus = FileInternalDeallocate((SFileInternal *) *psFile);

	*psFile = NULL;

errorExit:
   return(eStatus);
}

EStatus Filefgets(char *peStringBuffer, uint64_t u64MaxSize, SOSFile sFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) sFile;
	EStatus eStatus;

	eStatus = FileIsValid(sFile);
	if (eStatus != ESTATUS_OK)
	{
		return(eStatus);
	}

	return(PortFilefgets(peStringBuffer,
						 u64MaxSize,
						 psFileInternal->sFile));
}


EStatus Filerename(char *peOldFilename, char *peNewFilename)
{
	EStatus eStatus;
	char *peOldFilenameRebased = NULL;
	char *peNewFilenameRebased = NULL;
	char *peOld = NULL;
	char *peNew = NULL;

	// Get the old filename and rebase it
	eStatus = FileConstructAbsoluteFilename(peOldFilename,
											&peOldFilenameRebased);
	ERR_GOTO();

	peOld = peOldFilenameRebased;
	if (NULL == peOld)
	{
		peOld = peOldFilename;
	}

	// Now the new one
	eStatus = FileConstructAbsoluteFilename(peNewFilename,
											&peNewFilenameRebased);
	ERR_GOTO();

	peNew = peNewFilenameRebased;
	if (NULL == peNew)
	{
		peNew = peNewFilename;
	}

	// At this point, peOld is the old filename and peNew is the
	// new filename, rebased or not, but neither pointer should be
	// freed because it could point to the source (which isn't in the heap)
	// or to a freshly allocated block of memory.
	eStatus = PortFilerename(peOld,
							 peNew);

errorExit:
	if (peOldFilenameRebased)
	{
		MemFree(peOldFilenameRebased);
	}

	if (peNewFilenameRebased)
	{
		MemFree(peNewFilenameRebased);
	}

	return(eStatus);
}

EStatus Filemkdir(char *peDirName)
{
	EStatus eStatus = ESTATUS_OK;
	char *peTempDirname = NULL;

	eStatus = FileConstructAbsoluteFilename(peDirName,
											&peTempDirname);
	ERR_GOTO();

	if (peTempDirname)
	{
		// Rebased directory name
		eStatus = PortFilemkdir(peTempDirname);
		MemFree(peTempDirname);
	}
	else
	{
		// Just what we were passed in
		eStatus = PortFilemkdir(peDirName);
	}

errorExit:
	return(eStatus);
}

EStatus Filechdir(char *peDirName)
{
	return(PortFilechdir(peDirName));
}

EStatus Fileunlink(char *peFilename)
{
	EStatus eStatus;
	char *peTempFilename = NULL;

	eStatus = FileConstructAbsoluteFilename(peFilename,
											&peTempFilename);
	ERR_GOTO();

	if (peTempFilename)
	{
		// Rebased filename
		eStatus = PortFileunlink(peTempFilename);
		MemFree(peTempFilename);
	}
	else
	{
		// Just the incoming filename
		eStatus = PortFileunlink(peFilename);
	}

errorExit:
	return(eStatus);
}

EStatus Filermdir(char *peDirName)
{
	EStatus eStatus;
	char *peTempDirName = NULL;

	eStatus = FileConstructAbsoluteFilename(peDirName,
											&peTempDirName);
	ERR_GOTO();

	if (peTempDirName)
	{
		// Rebased directory name
		eStatus = PortFilermdir(peTempDirName);
		MemFree(peTempDirName);
	}
	else
	{
		// Just the incoming directory name
		eStatus = PortFilermdir(peDirName);
	}

errorExit:
	return(eStatus);
}

EStatus FileGetFree(uint64_t *pu64FreeSpace)
{
	return(PortFileGetFree(pu64FreeSpace));
}

EStatus FileSetFileTime(char *peFilename,
					    struct tm *psTime)
{
	return(PortFileSetFileTime(peFilename,
							   psTime));
}

EStatus FileStat(char *peFilename,
				 uint64_t *pu64Timestamp,
				 uint64_t *pu64FileSize,
				 uint32_t *pu32FileAttributes)
{
	return(PortFileStat(peFilename,
						pu64Timestamp,
						pu64FileSize,
						pu32FileAttributes));
}

uint64_t FileSize(SOSFile psFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) psFile;

	return(PortFileSize(psFileInternal->sFile));
}

EStatus FileSetAttributes(char *peFilename,
						  uint32_t u32Attributes)
{
	return(PortFileSetAttributes(peFilename,
								 u32Attributes));
}

EStatus FileFindOpen(char *pePath,
					 SFileFind **ppsFileFind)
{
	EStatus eStatus;
	char *peTempPath = NULL;

	eStatus = FileConstructAbsoluteFilename(pePath,
											&peTempPath);
	ERR_GOTO();

	if (peTempPath)
	{
		// Rebased directory name
		eStatus = PortFileFindOpen(peTempPath,
								   ppsFileFind);
		MemFree(peTempPath);
	}
	else
	{
		// Just the incoming directory name
		eStatus = PortFileFindOpen(pePath,
								   ppsFileFind);
	}
	
errorExit:
	return(eStatus);
}

EStatus FileFindFirst(char *pePath,
					  char *pePattern,
					  SFileFind **ppsFileFind)
{
	EStatus eStatus;
	char *peTempPath = NULL;

	eStatus = FileConstructAbsoluteFilename(pePath,
											&peTempPath);
	ERR_GOTO();

	if (peTempPath)
	{
		// Rebased directory name
		eStatus = PortFileFindFirst(peTempPath,
									pePattern,
								    ppsFileFind);
		MemFree(peTempPath);
	}
	else
	{
		// Just the incoming directory name
		eStatus = PortFileFindFirst(pePath,
									pePattern,
								   	ppsFileFind);
	}
	
errorExit:
	return(eStatus);
}

EStatus FileFindReadDir(SFileFind *psFileFind)
{
	return(PortFileFindReadDir(psFileFind));
}

EStatus FileFindNext(SFileFind *psFileFind)
{
	return(PortFileFindNext(psFileFind));
}

EStatus FileFindClose(SFileFind **ppsFileFind)
{
	return(PortFileFindClose(ppsFileFind));
}

EStatus FilesystemCreate(void)
{
	return(PortFilesystemCreate());
}

EStatus FilermdirTree(char *peDirName)
{
	EStatus eStatus;
	char *peTempDirName = NULL;

	eStatus = FileConstructAbsoluteFilename(peDirName,
											&peTempDirName);
	ERR_GOTO();

	if (peTempDirName)
	{
		// Rebased directory name
		eStatus = PortFilermdirTree(peTempDirName);
		MemFree(peTempDirName);
	}
	else
	{
		// Just the incoming directory name
		eStatus = PortFilermdirTree(peDirName);
	}

errorExit:
	return(eStatus);
}

bool Filefeof(SOSFile psFile)
{
	return(PortFilefeof(psFile));
}

#define	FPRINTF_MAX_LENGTH		2048

EStatus Filefprintf(SOSFile sFile,
					const char *peFormat,
					...)
{
	EStatus eStatus;
	va_list ap;
	size_t eLength;
	uint64_t u64DataWritten = 0;
	char eTempString[FPRINTF_MAX_LENGTH];

	eStatus = FileIsValid(sFile);
	ERR_GOTO();
   
	va_start(ap, peFormat);
	(void) vsnprintf(eTempString, FPRINTF_MAX_LENGTH - 1, (const char *) peFormat, ap);
	va_end(ap);
	
	eLength = strlen(eTempString);	

	// Go write the output data
	eStatus = Filefwrite(eTempString,
						 eLength,
						 &u64DataWritten,
						 sFile);
	
	if (ESTATUS_OK == eStatus)
	{
		if (u64DataWritten != eLength)
		{
			eStatus = ESTATUS_WRITE_TRUNCATED;
		}
		else
		{
			eStatus = ESTATUS_OK;
		}
	}
	else
	{
		// Disk write error
	}
						 
errorExit:
   
	return(eStatus);
}

EStatus FilefprintfCSV(SOSFile sFile,
					   const char *peFormat,
					   ...)
{
	EStatus eStatus;
	va_list ap;
	size_t eLength;
	uint64_t u64DataWritten = 0;
	char eTempString[FPRINTF_MAX_LENGTH];
	uint64_t u64QuoteCount = 0;
	uint64_t u64CommaCount = 0;
	char *peTemp;
	char *peTempHead = NULL;

	eStatus = FileIsValid(sFile);
	ERR_GOTO();
   
	va_start(ap, peFormat);
	(void) vsnprintf(eTempString, FPRINTF_MAX_LENGTH - 1, (const char *) peFormat, ap);
	va_end(ap);
	
	eLength = strlen(eTempString);	

	// Go count the quotes
	peTemp = eTempString;

	while (*peTemp)
	{
		if ('\"' == *peTemp)
		{
			u64QuoteCount++;
		}

		if (',' == *peTemp)
		{
			u64CommaCount++;
		}

		++peTemp;
	}

	if ((u64QuoteCount) ||
		(u64CommaCount))
	{
		char *peSrc;
		char *peTarget;

		// Need to CSV-safe it, dammit
		eLength +=  3 + u64QuoteCount + u64CommaCount;
		MEMALLOC(peTempHead, eLength);

		peTarget = peTempHead;
		*peTarget = '\"';
		++peTarget;

		peSrc = eTempString;
		while (*peSrc)
		{
			*peTarget = *peSrc;
			++peTarget;

			if ('"' == *peSrc)
			{
				*peTarget = *peSrc;
				++peTarget;
			}

			++peSrc;
		}

		*peTarget = '\"';
		++peTarget;
		*peTarget = 0;

		peTemp = peTempHead;
	}
	else
	{
		peTemp = eTempString;
	}

	// Go write the output data
	eStatus = Filefwrite(peTemp,
						 eLength,
						 &u64DataWritten,
						 sFile);
	
	if (ESTATUS_OK == eStatus)
	{
		if (u64DataWritten != eLength)
		{
			eStatus = ESTATUS_WRITE_TRUNCATED;
		}
		else
		{
			eStatus = ESTATUS_OK;
		}
	}
	else
	{
		// Disk write error
	}
						 
errorExit:
	SafeMemFree(peTempHead);
	return(eStatus);
}

EStatus FileCopy(char *peSourceFile,
				 char *peDestinationFile)
{
	EStatus eStatus;
	SOSFile sFileSrc;
	SOSFile sFileDest;
	uint8_t *pu8TempBuffer = NULL;
	
	eStatus = Filefopen(&sFileSrc,
					   peSourceFile,
					   "rb");
	ERR_GOTO();
	
	eStatus = Filefopen(&sFileDest,
					   peDestinationFile,
					   "wb");
	ERR_GOTO();
	
	// Both files open. Ready to copy
	pu8TempBuffer = MemAllocNoClear(FILE_COPY_BUFFER_SIZE);
	if (NULL == pu8TempBuffer)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExitClose;
	}
	
	for (;;)
	{
		uint64_t u64SizeRead;
		uint64_t u64SizeWritten;
		
		// Read a chunk
		eStatus = Filefread(pu8TempBuffer, FILE_COPY_BUFFER_SIZE, &u64SizeRead, sFileSrc);
		if (eStatus != ESTATUS_OK)
		{
			goto errorExitClose;
		}
		
		// If we've read 0, we're at the end of the file. Just break out.
		if (0 == u64SizeRead)
		{
			break;
		}
		
		// Write a chunk
		eStatus = Filefwrite(pu8TempBuffer, u64SizeRead, &u64SizeWritten, sFileDest);
		if (eStatus != ESTATUS_OK)
		{
			goto errorExitClose;
		}
		
		if (u64SizeRead != u64SizeWritten)
		{
			// Write truncation
			eStatus = ESTATUS_WRITE_TRUNCATED;
			goto errorExitClose;
		}
	}

errorExitClose:
	if (ESTATUS_OK == eStatus)
	{
		eStatus = Filefclose(&sFileSrc);
	}
	else
	{
		(void) Filefclose(&sFileSrc);
	}
	
	if (ESTATUS_OK == eStatus)
	{
		eStatus = Filefclose(&sFileDest);
	}
	else
	{
		(void) Filefclose(&sFileDest);
	}
	
errorExit:
	if (pu8TempBuffer)
	{
		MemFree(pu8TempBuffer);
	}
	
	return(eStatus);
}

EStatus FileMove(char *peSourceFile,
				 char *peDestinationFile)
{
	EStatus eStatus;
	
	eStatus = FileCopy(peSourceFile,
					   peDestinationFile);
	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}
	
	// Remove the source file
	eStatus = Fileunlink(peSourceFile);
	
errorExit:
	return(eStatus);
}

EStatus Filefflush(SOSFile psFile)
{
	SFileInternal *psFileInternal = (SFileInternal *) psFile;
	EStatus eStatus;

	eStatus = FileIsValid(psFile);
	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}

	return(PortFilefflush(psFileInternal->sFile));

errorExit:
	return(eStatus);
}

static EStatus FileLoadInternal(char *peFilename,
								uint8_t **ppu8FileData,
								uint64_t *pu64FileLength,
								bool bEncrypted,
								uint64_t u64AdditionalAlloc)
{
	EStatus eStatus;
	SOSFile sFile;
	bool bFileOpened = false;

	// Open up the file - encrypted or otherwise
	if (false == bEncrypted)
	{
		eStatus = Filefopen(&sFile,
						   peFilename, "rb");
	}
	else
	{
		eStatus = FileOpenEncrypted(&sFile,
									peFilename,
									"rb");
	}

	ERR_GOTO();
	bFileOpened = true;

	// Record the file size
	*pu64FileLength = FileSize(sFile);

	// Special case - zero length file check
	if (0 == *pu64FileLength)
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	*ppu8FileData = MemAllocNoClear(*pu64FileLength + u64AdditionalAlloc);
	if (NULL == *ppu8FileData)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	eStatus = Filefread((void *) *ppu8FileData, *pu64FileLength, NULL, sFile);

errorExit:
	if (bFileOpened)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = Filefclose(&sFile);
		}
		else
		{
			SafeMemFree(*ppu8FileData);
			(void) Filefclose(&sFile);
		}
	}

	return(eStatus);
}

EStatus FileLoad(char *peFilename,
				 uint8_t **ppu8FileData,
				 uint64_t *pu64FileLength,
				 uint64_t u64AdditionalAlloc)
{
	return(FileLoadInternal(peFilename,
							ppu8FileData,
							pu64FileLength,
							false,
							u64AdditionalAlloc));
}

EStatus FileLoadEncrypted(char *peFilename,
						  uint8_t **ppu8FileData,
						  uint64_t *pu64FileLength)
{
	return(FileLoadInternal(peFilename,
							ppu8FileData,
							pu64FileLength,
							true,
							0));
}

EStatus FileWriteFromMemory(char *peFilename,
							uint8_t *pu8FileData,
							uint64_t u64FileSize)
{
	SOSFile psFile;
	uint64_t u64SizeWritten;
	EStatus eStatus;

	// Open for binary write
	eStatus = Filefopen(&psFile,
					   peFilename,
					   "wb");
	ERR_GOTO();

	// Write the whole file
	eStatus = Filefwrite((void *) pu8FileData,
						 u64FileSize,
						 &u64SizeWritten,
						 psFile);

	// Successful. Maybe
	if (ESTATUS_OK == eStatus)
	{
		eStatus = Filefclose(&psFile);
	}
	else
	{
		(void) Filefclose(&psFile);
	}
	
	if (u64SizeWritten != (size_t) u64FileSize)
	{
		eStatus = ESTATUS_WRITE_TRUNCATED;
	}

errorExit:
	return(eStatus);
}

EStatus FileWriteFromMemoryAndVerify(char *peFilename,
									 uint8_t *pu8FileData,
									 uint64_t u64FileSize)
{
	uint64_t u64FileSizeRead = 0;
	uint8_t *pu8ReadData = NULL;
	EStatus eStatus;

	// Write the image to disk
	eStatus = FileWriteFromMemory(peFilename,
								  pu8FileData,
								  u64FileSize);
	ERR_GOTO();

	// File written. Now load it back in.
	eStatus = FileLoad(peFilename,
					  &pu8ReadData,
					  &u64FileSizeRead,
					  0);
	ERR_GOTO();

	// File read in properly. Let's see if the size matches.
	if (u64FileSizeRead != u64FileSize)
	{
		SyslogFunc("File '%s' readback size mismatch - read %lluu bytes, expected %llu bytes\n", 
				   peFilename, 
				   u64FileSizeRead, 
				   u64FileSize);
		eStatus = ESTATUS_READ_TRUNCATED;
		goto errorExit;
	}

	// File size is correct. Let's do a memory compare on it
	if (memcmp(pu8FileData, pu8ReadData, (size_t) u64FileSize))
	{
		DebugOutFunc("New file '%s' and in-memory image mismatch after readback\n", 
					 peFilename);
		eStatus = ESTATUS_DATA_MISCOMPARE;
		goto errorExit;
	}

	// All good.
	eStatus = ESTATUS_OK;

errorExit:
	SafeMemFree(pu8ReadData);
	return(eStatus);
}