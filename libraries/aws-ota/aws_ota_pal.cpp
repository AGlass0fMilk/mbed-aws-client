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

#include "aws_ota_pal_flash.h"

#define TRACE_GROUP "ota_pal"

/* Specify the OTA signature algorithm we support on this platform. */
const char cOTA_JSON_FileSignatureKey[ OTA_FILE_SIG_KEY_STR_MAX_LENGTH ] = "sig-sha256-ecdsa";

static inline bool prvContextValidate( OTA_FileContext_t * C )
{
    return( ( C != NULL ) &&
            ( C->lFileHandle != NULL ) ); /*lint !e9034 Comparison is correct for file pointer type. */
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
        eResult = kOTA_Err_None;
        /* Close the OTA update file if it's open. */
        // TODO - close file?
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
    mbed::BlockDevice* update_bd = NULL;
    int err = 0;

    if( C != NULL )
    {
        eResult = kOTA_Err_None;

        // Update flash area already opened and initialized
        if( C->lFileHandle != NULL ) {
            eResult = kOTA_Err_RxFileCreateFailed;
        }

        if(eResult == kOTA_Err_None) {
            // Get the flash update partition block device
            update_bd = aws::ota::get_update_bd();
            if(update_bd == NULL) {
                OTA_LOG_L1( "[%s] ERROR - Null update block device pointer!\r\n", OTA_METHOD_NAME );
                eResult = kOTA_Err_RxFileCreateFailed;
            }
        }


        if(eResult == kOTA_Err_None) {
            // Initialize the update block device
            err = update_bd->init();
            if(err) {
                OTA_LOG_L1( "[%s] ERROR - Failed to initialize update block device!\r\n", OTA_METHOD_NAME );
                eResult = kOTA_Err_RxFileCreateFailed;
            }
        }

        if(eResult == kOTA_Err_None) {

            // Erase the update block device
            err = update_bd->erase(0, update_bd->size());

            if(err) {
                OTA_LOG_L1( "[%s] ERROR - Failed to erase the update block device!\r\n", OTA_METHOD_NAME );
                eResult = kOTA_Err_RxFileCreateFailed;
            }
        }

        if(eResult == kOTA_Err_None) {
            // Check that the update partition is big enough to contain the new firmware
            if(C->ulFileSize > update_bd->size()) {
                OTA_LOG_L1( "[%s] ERROR - Update size is larger than update block device (%i > %i)\r\n",
                        OTA_METHOD_NAME, C->ulFileSize, update_bd->size());
                eResult = kOTA_Err_OutOfMemory;
            }

            // Save the update_bd pointer and return OK
            C->lFileHandle = (int32_t)(update_bd);
        }
    }

    return eResult; /*lint !e480 !e481 Exiting function without calling fclose.
                     * Context file handle state is managed by this API. */
}

OTA_Err_t prvPAL_CloseFile(OTA_FileContext_t* const C) {
    DEFINE_OTA_METHOD_NAME( "prvPAL_CloseFile" );

    OTA_Err_t eResult = kOTA_Err_None;

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

        // TODO flush all writes and handling close update bd, resetting state, etc

        if( eResult == kOTA_Err_None )
        {
            // Flag the image for mcuboot
            // TODO maybe put this in ActivateNewImage?
            aws::ota::flag_update_as_ready();

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
        // Cast to a block device pointer
        mbed::BlockDevice* update_bd = (mbed::BlockDevice*)(C->lFileHandle);
        lResult = update_bd->program(pcData, ulOffset, ulBlockSize);
        if(lResult < 0) {
            OTA_LOG_L1( "[%s] ERROR - flash program failed\r\n", OTA_METHOD_NAME );
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


