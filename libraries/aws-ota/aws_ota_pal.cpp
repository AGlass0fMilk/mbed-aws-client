/*
 * aws_ota_pal.cpp
 *
 *  Created on: Jul 21, 2020
 *      Author: gdbeckstein
 */

#include "aws_iot_ota_pal.h"

#include <cstdio>
#include "errno.h"

#include "platform/mbed_power_mgmt.h"

#include "mbed_trace.h"

#define TRACE_GROUP "ota_pal"

/* Specify the OTA signature algorithm we support on this platform. */
const char cOTA_JSON_FileSignatureKey[ OTA_FILE_SIG_KEY_STR_MAX_LENGTH ] = "sig-sha256-ecdsa";

static inline bool prvContextValidate( OTA_FileContext_t * C )
{
    return( ( C != NULL ) &&
            ( C->pucFile != NULL ) ); /*lint !e9034 Comparison is correct for file pointer type. */
}

/* Used to set the high bit of Windows error codes for a negative return value. */
#define OTA_PAL_INT16_NEGATIVE_MASK    ( 1 << 15 )

/* Size of buffer used in file operations on this platform (Windows). */
#define OTA_PAL_WIN_BUF_SIZE ( ( size_t ) 4096UL )

OTA_Err_t prvPAL_Abort(OTA_FileContext_t* const C) {
    DEFINE_OTA_METHOD_NAME( "prvPAL_Abort" );

    /* Set default return status to uninitialized. */
    OTA_Err_t eResult = kOTA_Err_Uninitialized;
    int32_t lFileCloseResult;

    if( NULL != C )
    {
        /* Close the OTA update file if it's open. */
        if( NULL != C->pucFile )
        {
            lFileCloseResult = fclose((__sFILE*)C->pucFile ); /*lint !e482 !e586
                                                      * Context file handle state is managed by this API. */
            C->pucFile = NULL;

            if( 0 == lFileCloseResult )
            {
                OTA_LOG_L1( "[%s] OK\r\n", OTA_METHOD_NAME );
                eResult = kOTA_Err_None;
            }
            else /* Failed to close file. */
            {
                OTA_LOG_L1( "[%s] ERROR - Closing file failed.\r\n", OTA_METHOD_NAME );
                eResult = ( kOTA_Err_FileAbort | ( errno & kOTA_PAL_ErrMask ) );
            }
        }
        else
        {
            /* Nothing to do. No open file associated with this context. */
            eResult = kOTA_Err_None;
        }
    }
    else /* Context was not valid. */
    {
        OTA_LOG_L1( "[%s] ERROR - Invalid context.\r\n", OTA_METHOD_NAME );
        eResult = kOTA_Err_FileAbort;
    }

    return eResult;
}

OTA_Err_t prvPAL_CreateFileForRx(OTA_FileContext_t* const C) {
    DEFINE_OTA_METHOD_NAME( "prvPAL_CreateFileForRx" );

    OTA_Err_t eResult = kOTA_Err_Uninitialized; /* For MISRA mandatory. */

    if( C != NULL )
    {
        if ( C->pucFilePath != NULL )
        {
            C->pucFile = (uint8_t*) fopen( ( const char * )C->pucFilePath, "w+b" ); /*lint !e586
                                                                           * C standard library call is being used for portability. */

            if ( C->pucFile != NULL )
            {
                eResult = kOTA_Err_None;
                OTA_LOG_L1( "[%s] Receive file created.\r\n", OTA_METHOD_NAME );
            }
            else
            {
                eResult = ( kOTA_Err_RxFileCreateFailed | ( errno & kOTA_PAL_ErrMask ) ); /*lint !e40 !e737 !e9027 !e9029 */
                OTA_LOG_L1( "[%s] ERROR - Failed to start operation: already active!\r\n", OTA_METHOD_NAME );
            }
        }
        else
        {
            eResult = kOTA_Err_RxFileCreateFailed;
            OTA_LOG_L1( "[%s] ERROR - Invalid context provided.\r\n", OTA_METHOD_NAME );
        }
    }
    else
    {
        eResult = kOTA_Err_RxFileCreateFailed;
        OTA_LOG_L1( "[%s] ERROR - Invalid context provided.\r\n", OTA_METHOD_NAME );
    }

    return eResult; /*lint !e480 !e481 Exiting function without calling fclose.
                     * Context file handle state is managed by this API. */
}

OTA_Err_t prvPAL_CloseFile(OTA_FileContext_t* const C) {
    DEFINE_OTA_METHOD_NAME( "prvPAL_CloseFile" );

    OTA_Err_t eResult = kOTA_Err_None;
    int32_t lWindowsError = 0;

    if( prvContextValidate( C ) == true )
    {
        if( C->pxSignature != NULL )
        {
            /* Verify the file signature, close the file and return the signature verification result. */
            // TODO - verify signature
            eResult = kOTA_Err_None;//prvPAL_CheckFileSignature( C );
        }
        else
        {
            OTA_LOG_L1( "[%s] ERROR - NULL OTA Signature structure.\r\n", OTA_METHOD_NAME );
            eResult = kOTA_Err_SignatureCheckFailed;
        }

        /* Close the file. */
        lWindowsError = fclose((__sFILE*) C->pucFile ); /*lint !e482 !e586
                                               * C standard library call is being used for portability. */
        C->pucFile = NULL;

        if( lWindowsError != 0 )
        {
            OTA_LOG_L1( "[%s] ERROR - Failed to close OTA update file.\r\n", OTA_METHOD_NAME );
            eResult = ( kOTA_Err_FileClose | ( errno & kOTA_PAL_ErrMask ) ); /*lint !e40 !e737 !e9027 !e9029
                                                                              * Errno is being used in accordance with host API documentation.
                                                                              * Bitmasking is being used to preserve host API error with library status code. */
        }

        if( eResult == kOTA_Err_None )
        {
            OTA_LOG_L1( "[%s] %s signature verification passed.\r\n", OTA_METHOD_NAME, cOTA_JSON_FileSignatureKey );
        }
        else
        {
            OTA_LOG_L1( "[%s] ERROR - Failed to pass %s signature verification: %d.\r\n", OTA_METHOD_NAME,
                        cOTA_JSON_FileSignatureKey, eResult );

            /* If we fail to verify the file signature that means the image is not valid. We need to set the image state to aborted. */
            prvPAL_SetPlatformImageState( eOTA_ImageState_Aborted );

        }
    }
    else /* Invalid OTA Context. */
    {
        /* FIXME: Invalid error code for a null file context and file handle. */
        OTA_LOG_L1( "[%s] ERROR - Invalid context.\r\n", OTA_METHOD_NAME );
        eResult = kOTA_Err_FileClose;
    }

    return eResult;
}

int16_t prvPAL_WriteBlock(OTA_FileContext_t* const C, uint32_t ulOffset,
        uint8_t* const pcData, uint32_t ulBlockSize) {
    DEFINE_OTA_METHOD_NAME( "prvPAL_WriteBlock" );

    int32_t lResult = 0;

    if( prvContextValidate( C ) == true )
    {
        lResult = fseek( (__sFILE*) C->pucFile, ulOffset, SEEK_SET ); /*lint !e586 !e713 !e9034
                                                            * C standard library call is being used for portability. */

        if( 0 == lResult )
        {
            lResult = fwrite( pcData, 1, ulBlockSize, (__sFILE*) C->pucFile ); /*lint !e586 !e713 !e9034
                                                                      * C standard library call is being used for portability. */

            if( lResult < 0 )
            {
                OTA_LOG_L1( "[%s] ERROR - fwrite failed\r\n", OTA_METHOD_NAME );
                /* Mask to return a negative value. */
                lResult = OTA_PAL_INT16_NEGATIVE_MASK | errno; /*lint !e40 !e9027
                                                                * Errno is being used in accordance with host API documentation.
                                                                * Bitmasking is being used to preserve host API error with library status code. */
            }
        }
        else
        {
            OTA_LOG_L1( "[%s] ERROR - fseek failed\r\n", OTA_METHOD_NAME );
            /* Mask to return a negative value. */
            lResult = OTA_PAL_INT16_NEGATIVE_MASK | errno; /*lint !e40 !e9027
                                                            * Errno is being used in accordance with host API documentation.
                                                            * Bitmasking is being used to preserve host API error with library status code. */
        }
    }
    else /* Invalid context or file pointer provided. */
    {
        OTA_LOG_L1( "[%s] ERROR - Invalid context.\r\n", OTA_METHOD_NAME );
        lResult = -1; /*TODO: Need a negative error code from the PAL here. */
    }

    return ( int16_t ) lResult;
}

OTA_Err_t prvPAL_ActivateNewImage(void) {
    prvPAL_ResetDevice();
    return kOTA_Err_Uninitialized;
}

OTA_Err_t prvPAL_ResetDevice(void) {
    system_reset();
    // Should never reach this
    return kOTA_Err_ResetNotSupported;
}

OTA_Err_t prvPAL_SetPlatformImageState(OTA_ImageState_t eState) {
    // TODO - stub
    return kOTA_Err_None;
}

OTA_PAL_ImageState_t prvPAL_GetPlatformImageState(void) {
    // TODO - stub
    return eOTA_PAL_ImageState_Valid;
}


