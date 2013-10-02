/*
 * Copyright (C) 2013 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#ifndef MERCURY_PROC_HEADER_H
#define MERCURY_PROC_HEADER_H

#include "mercury_proc.h"

typedef struct hg_header_request {
     hg_uint8_t  hg;               /* Mercury identifier */
     hg_uint32_t protocol;         /* Version number */
     hg_id_t     id;               /* RPC request identifier */
     hg_uint8_t  flags;            /* Flags (extra buffer) */
     hg_uint32_t cookie;           /* Random cookie */
     hg_uint16_t crc16;            /* CRC16 checksum */
     /* Should be 128 bits here */
     hg_bulk_t   extra_buf_handle; /* Extra handle (large data) */
} hg_header_request_t;

typedef struct hg_header_response {
    hg_uint8_t  flags;  /* Flags */
    hg_error_t  error;  /* Error */
    hg_uint32_t cookie; /* Cookie */
    hg_uint16_t crc16;  /* CRC16 checksum */
    hg_uint8_t  padding;
    /* Should be 96 bits here */
} hg_header_response_t;

/*
 * 0      HG_PROC_HEADER_SIZE              size
 * |______________|__________________________|
 * |    Header    |        Encoded Data      |
 * |______________|__________________________|
 *
 *
 * Request:
 * mercury byte / protocol version number / rpc id / flags (e.g. for extra buf) /
 * random cookie / crc16 / (bulk handle, there is space since payload is copied)
 *
 * Response:
 * flags / error / cookie / crc16 / payload
 */

/* Mercury identifier for packets sent */
#define HG_IDENTIFIER (('H' << 1) | ('G')) /* 0xD7 */

/* Encode/decode version number into uint32 */
#define HG_GET_MAJOR(value) ((value >> 24) & 0xFF)
#define HG_GET_MINOR(value) ((value >> 16) & 0xFF)
#define HG_GET_PATCH(value) (value & 0xFFFF)
#define HG_VERSION ((HG_VERSION_MAJOR << 24) | (HG_VERSION_MINOR << 16) \
        | HG_VERSION_PATCH)

#ifndef HG_PROC_HEADER_INLINE
  #if defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__)
    #define HG_PROC_HEADER_INLINE extern HG_INLINE
  #else
    #define HG_PROC_HEADER_INLINE HG_INLINE
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

HG_EXPORT HG_PROC_HEADER_INLINE size_t hg_proc_header_request_get_size(void);
HG_EXPORT HG_PROC_HEADER_INLINE size_t hg_proc_header_response_get_size(void);

/**
 * Get size reserved for request header (separate user data stored in payload).
 *
 * \return Non-negative size value
 */
HG_PROC_HEADER_INLINE size_t
hg_proc_header_request_get_size(void)
{
    /* hg_bulk_t is optional and is not really part of the header */
    return (sizeof(hg_header_request_t) - sizeof(hg_bulk_t));
}

/**
 * Get size reserved for response header (separate user data stored in payload).
 *
 * \return Non-negative size value
 */
HG_PROC_HEADER_INLINE size_t
hg_proc_header_response_get_size(void)
{
    return sizeof(hg_header_response_t);
}

/**
 * Initialize RPC request header.
 *
 * \param id [IN]               registered function ID
 * \param extra_buf_handle [IN] extra bulk handle
 * \param header [IN/OUT]       pointer to request header structure
 *
 */
HG_EXPORT void
hg_proc_header_request_init(hg_id_t id, hg_bulk_t extra_buf_handle,
        hg_header_request_t *header);

/**
 * Initialize RPC response header.
 *
 * \param header [IN/OUT]       pointer to response header structure
 *
 */
HG_EXPORT void
hg_proc_header_response_init(hg_header_response_t *header);


/**
 * Process private information for sending/receiving RPC request.
 *
 * \param proc [IN/OUT]         abstract processor object
 * \param header [IN/OUT]       pointer to header structure
 *
 * \return Non-negative on success or negative on failure
 */
HG_EXPORT int
hg_proc_header_request(hg_proc_t proc, hg_header_request_t *header);

/**
 * Process private information for sending/receiving response.
 *
 * \param proc [IN/OUT]         abstract processor object
 * \param header [IN/OUT]       pointer to header structure
 *
 * \return Non-negative on success or negative on failure
 */
HG_EXPORT int
hg_proc_header_response(hg_proc_t proc, hg_header_response_t *header);

/**
 * Verify private information from request header.
 *
 * \param header [IN]           request header structure
 *
 * \return Non-negative on success or negative on failure
 */
HG_EXPORT int
hg_proc_header_request_verify(hg_header_request_t header);

/**
 * Verify private information from response header.
 *
 * \param header [IN]           response header structure
 *
 * \return Non-negative on success or negative on failure
 */
HG_EXPORT int
hg_proc_header_response_verify(hg_header_response_t header);

#ifdef __cplusplus
}
#endif

#endif /* MERCURY_PROC_HEADER_H */