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

#ifndef __IMAGE_TABLE_H__
#define __IMAGE_TABLE_H__

#include "compiler.h"

#define IMAGE_SET_SIGNATURE 0x9db77c10
#define IMAGE_SET_DESCRIPTORS_COUNT 16
#define IMAGE_SET_RESERVED_BYTE 0xff

#define IMAGE_TYPE_INVALID    0xffffffff
#define IMAGE_TYPE_UNKNOWN    0x00000000
#define IMAGE_TYPE_LOADER     0x00000001
#define IMAGE_TYPE_UBOOT_SPL  0x00000002
#define IMAGE_TYPE_UBOOT      0x00000003
#define IMAGE_TYPE_LINUX      0x00000004
#define IMAGE_TYPE_FPGA       0x00000005

#define IMAGE_HARDWARE_INVALID      0xffffffff
#define IMAGE_HARDWARE_UNKNOWN      0x00000000
#define IMAGE_HARDWARE_V3_MICROZED  0x00000001
#define IMAGE_HARDWARE_V3_EVT1      0x00000011
#define IMAGE_HARDWARE_V3_EVT2      0x00000012
#define IMAGE_HARDWARE_V3_PROD      0x00000013

/* Warning: image_set_t and image_descriptor_t use unspecified endianness.
 * Do not access fields directly. Use API functions only. */
typedef struct {
  uint32_t _type;
  uint32_t _version;
  uint32_t _timestamp;
  uint32_t _load_address;
  uint32_t _entry_address;
  uint32_t _data_offset;
  uint32_t _data_size;
  uint32_t _data_crc;
  uint8_t  _name[32];
  uint32_t _reserved0[16];
} image_descriptor_t;

typedef struct {
  uint32_t _signature;
  uint32_t _version;
  uint32_t _timestamp;
  uint32_t _hardware;
  uint8_t  _name[32];
  uint32_t _reserved0[20];
  image_descriptor_t descriptors[IMAGE_SET_DESCRIPTORS_COUNT];
  uint32_t _reserved1[30];
  uint32_t _seq_num;
  uint32_t _crc;
} image_set_t;

typedef struct {
  uint32_t type;
  uint32_t version;
  uint32_t timestamp;
  uint32_t load_address;
  uint32_t entry_address;
  uint32_t data_offset;
  uint32_t data_size;
  uint32_t data_crc;
  uint8_t  name[32];
} image_descriptor_params_t;

typedef struct {
  uint32_t version;
  uint32_t timestamp;
  uint32_t seq_num;
  uint32_t hardware;
  uint8_t  name[32];
} image_set_params_t;

#define IMAGE_TABLE_GET_SET_U32_FN(type, param) \
  static inline uint32_t \
  image_##type##_##param##_get(const image_##type##_t *t) \
  { \
    return le32_to_cpu(t->_##param); \
  } \
  \
  static inline void \
  image_##type##_##param##_set(image_##type##_t *t, uint32_t value) \
  { \
    t->_##param = cpu_to_le32(value); \
  }

#define IMAGE_TABLE_GET_SET_STR_FN(type, param) \
  static inline void \
  image_##type##_##param##_get(const image_##type##_t *t, uint8_t *value) \
  { \
    memcpy(value, t->_##param, sizeof(t->_##param)); \
  } \
  \
  static inline void \
  image_##type##_##param##_set(image_##type##_t *t, const uint8_t *value) \
  { \
    memcpy(t->_##param, value, sizeof(t->_##param)); \
  }

IMAGE_TABLE_GET_SET_U32_FN(set, version);
IMAGE_TABLE_GET_SET_U32_FN(set, timestamp);
IMAGE_TABLE_GET_SET_U32_FN(set, seq_num);
IMAGE_TABLE_GET_SET_U32_FN(set, hardware);
IMAGE_TABLE_GET_SET_STR_FN(set, name);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, type);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, version);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, timestamp);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, load_address);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, entry_address);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, data_offset);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, data_size);
IMAGE_TABLE_GET_SET_U32_FN(descriptor, data_crc);
IMAGE_TABLE_GET_SET_STR_FN(descriptor, name);

int image_set_verify(const image_set_t *s);
int image_set_descriptor_find(const image_set_t *s, uint32_t image_type,
                              const image_descriptor_t **d);

void image_set_init(image_set_t *s, const image_set_params_t *p);
int image_set_descriptor_add(image_set_t *s,
                             const image_descriptor_params_t *p);
void image_set_finalize(image_set_t *s);

void image_descriptor_data_crc_init(uint32_t *crc);
void image_descriptor_data_crc_continue(uint32_t *crc, const uint8_t *data,
                                        uint32_t data_length);

#endif /* __IMAGE_TABLE_H__ */
