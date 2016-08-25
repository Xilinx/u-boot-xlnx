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

#define IMAGE_SET_SIGNATURE 0x9db77c10
#define IMAGE_SET_DESCRIPTORS_COUNT 16

#define IMAGE_TYPE_INVALID    0x00000000
#define IMAGE_TYPE_LOADER     0x00000001
#define IMAGE_TYPE_UBOOT_SPL  0x00000002
#define IMAGE_TYPE_UBOOT      0x00000003
#define IMAGE_TYPE_LINUX      0x00000004

typedef struct {
  uint32_t type;
  uint32_t version;
  uint32_t timestamp;
  uint32_t load_address;
  uint32_t data_offset;
  uint32_t data_length;
  uint32_t data_crc;
  uint32_t reserved0[25];
} image_descriptor_t;

typedef struct {
  uint32_t signature;
  uint32_t version;
  uint32_t timestamp;
  uint32_t reserved0[29];
  image_descriptor_t descriptors[IMAGE_SET_DESCRIPTORS_COUNT];
  uint32_t reserved1[30];
  uint32_t seq_num;
  uint32_t crc;
} image_set_t;

int image_set_verify(const image_set_t *s);
int image_set_find_descriptor(const image_set_t *s, uint32_t image_type,
                              const image_descriptor_t **d);

#endif /* __IMAGE_TABLE_H__ */
