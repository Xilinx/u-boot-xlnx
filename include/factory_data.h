/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __FACTORY_DATA_H__
#define __FACTORY_DATA_H__

#include "compiler.h"

/* Factory data storage
 * - Versioned, backwards-compatible structures
 * - Header: fixed size, reserved fields may be repurposed
 * - Data: variable size, fields may be appended but never removed
 */

#define FACTORY_DATA_SIGNATURE 0xcb1e6082
#define FACTORY_DATA_RESERVED_BYTE 0xff

/* Warning: factory data structures use unspecified endianness.
 * Do not access fields directly. Use API functions only. */
typedef struct {
  uint32_t _hardware;
  uint8_t _mfg_serial_number[17];
  uint8_t _uuid[16];
  uint32_t _timestamp;
  uint8_t  _nap_key[16];
  uint8_t  _mac_address[6];
  uint32_t _factory_stage;
  uint8_t  _reserved0[2];
} factory_data_body_t;

typedef struct {
  uint32_t _signature;
  uint32_t _body_size;
  uint32_t _body_crc;
  uint32_t _reserved[12];
  uint32_t _header_crc;
  factory_data_body_t body[0]; /* variable length */
} factory_data_t;

typedef struct {
  uint32_t hardware;
  uint8_t mfg_serial_number[17];
  uint8_t uuid[16];
  uint32_t timestamp;
  uint8_t nap_key[16];
  uint8_t mac_address[6];
  uint32_t factory_stage;
} factory_data_params_t;

static inline uint32_t factory_data_body_size_get(const factory_data_t *f) {
  return le32_to_cpu(f->_body_size);
}

#define FACTORY_DATA_GET_U32_FN(param) \
  static inline int \
  factory_data_##param##_get(const factory_data_t *f, uint32_t *value) \
  { \
    const factory_data_body_t *b = (const factory_data_body_t *)&f->body[0]; \
    if (le32_to_cpu(f->_body_size) < \
            offsetof(factory_data_body_t, _##param) + sizeof(b->_##param)) { \
      return -1; \
    } \
    *value = le32_to_cpu(b->_##param); \
    return 0; \
  }

#define FACTORY_DATA_GET_ARRAY_FN(param) \
  static inline int \
  factory_data_##param##_get(const factory_data_t *f, uint8_t *value) \
  { \
    const factory_data_body_t *b = (const factory_data_body_t *)&f->body[0]; \
    if (le32_to_cpu(f->_body_size) < \
            offsetof(factory_data_body_t, _##param) + sizeof(b->_##param)) { \
      return -1; \
    } \
    memcpy(value, b->_##param, sizeof(b->_##param)); \
    return 0; \
  }

FACTORY_DATA_GET_U32_FN(hardware);
FACTORY_DATA_GET_ARRAY_FN(mfg_serial_number);
FACTORY_DATA_GET_ARRAY_FN(uuid);
FACTORY_DATA_GET_U32_FN(timestamp);
FACTORY_DATA_GET_ARRAY_FN(nap_key);
FACTORY_DATA_GET_ARRAY_FN(mac_address);
FACTORY_DATA_GET_U32_FN(factory_stage);

int factory_data_header_verify(const factory_data_t *f);
int factory_data_body_verify(const factory_data_t *f);

int factory_data_populate(factory_data_t *f, const factory_data_params_t *p);

#endif /* __FACTORY_DATA_H__ */
