/*
 * File Name       :CommonHeader.h
 *
 * A common header file to be reused in many projects
 *
 */
#ifndef COMMONHEADER_H
#define COMMONHEADER_H

#ifdef __cplusplus
extern "C" {
#endif

//#include <iostream>
#include <stdarg.h>
#include <sys/time.h>


/*
 * Common interface return type
 */

/*! Genral Success */
#define C_SUCCESS                  0
/*! General failure */
#define C_FAILURE                  -1
/*! Invalid handle */
#define C_INVALID_HANDLE           -2
/*! Invalid arguments */
#define C_INVALID_ARGS             -3
/*! Out of memory */
#define C_MALLOC_FAILED            -4
/*! Feature is Unsupported */
#define C_UNSUPPORTED              -5
/*! Data EOF */
#define C_DATA_EOF                 -6
/*! General Timeout */
#define C_TIMEOUT                  -7

/*
 * Typedef for Common API return types.
 */
typedef int              CStatus;

#define C_ERROR                    100
#define C_ASSERT                    90
#define C_WARNING                   80
#define C_INFO                      70
#define C_VERBOSE                   60

#if (DEBUG)
#define C_LEVEL                    C_VERBOSE
#else
#define C_LEVEL                    C_INFO
#endif

#define DLOG(LEVEL, str, ...)                                                     \
    do {                                                                          \
        if (LEVEL >= C_LEVEL)                                                     \
       	printf("[%s][%s][%d] " str, __FILE__, __func__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define CLOG_ASSERT_ERR(ASSERT_STRING, MESSAGE, ERRORCODE)                                   \
    do {                                                                                     \
       	printf("!!!!! [ASSERT] [%s][%s][%d] %s err: %d, %s\n", __FILE__, __func__, __LINE__, \
				ASSERT_STRING, ERRORCODE, MESSAGE); \
    } while (0)


/*
 *  Check for success and print debug
 */
#ifndef CHK_SUCCESS_NOEXIT
#define CHK_SUCCESS_NOEXIT(ERR)                                                                \
do {                                                                                           \
    int localErr;                                                                              \
    localErr = (int)(ERR);                                                                     \
                                                                                               \
    if (localErr != 0) {                                                                       \
        CLOG_ASSERT_ERR ("", "", localErr);                                                    \
    }                                                                                          \
}   while (0)
#endif

/*
 *  Check for success and print debug string and jump to label
 */
#ifndef CHK_SUCCESS_STR
#define CHK_SUCCESS_STR(ERR, LABEL, STR)                                                       \
do {                                                                                           \
    int localErr;                                                                              \
    localErr = (int)(ERR);                                                                     \
                                                                                               \
    if (localErr != 0) {                                                                       \
        CLOG_ASSERT_ERR ("", STR, localErr);                                                   \
        goto LABEL;                                                                            \
    }                                                                                          \
}   while (0)
#endif



#ifdef __cplusplus
}
#endif



#endif /* COMMONHEADER_H */
