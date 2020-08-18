/*
 * aws_ota_pal_flash.h
 *
 *  Created on: Aug 11, 2020
 *      Author: gdbeckstein
 */

#ifndef MBED_AWS_CLIENT_LIBRARIES_AWS_OTA_AWS_OTA_PAL_FLASH_H_
#define MBED_AWS_CLIENT_LIBRARIES_AWS_OTA_AWS_OTA_PAL_FLASH_H_

#include "BlockDevice.h"

namespace aws
{

namespace ota {

/**
 * Returns a pointer to the blockdevice to be used
 * for OTA updates.
 *
 * @note The application is responsible for implementing this function call.
 *
 * @retval update_bd BlockDevice pointer to use for OTA updates
 *
 */
mbed::BlockDevice* get_update_bd(void);

/**
 * Flags the update as ready to boot
 */
void flag_update_as_ready(void);

}

}


#endif /* MBED_AWS_CLIENT_LIBRARIES_AWS_OTA_AWS_OTA_PAL_FLASH_H_ */
