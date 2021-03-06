/*
 * Copyright (C) 2020 ARM, Ltd. or its affiliates.
 */

/**
 * @file iot_platform_types_template.h
 * @brief Template definitions of platform layer types.
 */

#ifndef IOT_PLATFORM_TYPES_MBED_OS_H_
#define IOT_PLATFORM_TYPES_MBED_OS_H_

#include <stdint.h>

/**
 * @brief Set this to the target system's mutex type.
 */
struct SystemMutex;
typedef struct SystemMutex * _IotSystemMutex_t;

/**
 * @brief Set this to the target system's semaphore type.
 */
struct SystemSemaphore;
typedef struct SystemSemaphore *_IotSystemSemaphore_t;

/**
 * @brief Set this to the target system's timer type.
 */
struct SystemTimer;
typedef struct SystemTimer *_IotSystemTimer_t;

/**
 * @brief The format for remote server host and port on this system.
 */
typedef struct {
    const char *hostname;
    uint16_t port;
} _IotNetworkServerInfo_t;

/**
 * @brief The format for network credentials on this system.
 */
typedef struct {
    const char *rootCA;
    const char *clientCrt;
    const char *clientKey;
} _IotNetworkCredentials_t;

/**
 * @brief The handle of a network connection on this system.
 */
struct NetworkConnection;
typedef struct NetworkConnection * _IotNetworkConnection_t;

#endif /* ifndef IOT_PLATFORM_TYPES_TEMPLATE_H_ */
