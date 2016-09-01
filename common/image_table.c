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

#include <common.h>
#include "image_table.h"

int image_set_verify(const image_set_t *s)
{
  if (s->signature != IMAGE_SET_SIGNATURE) {
    return -1;
  }

  uint32_t crc = crc32(0, (const unsigned char *)s,
                       sizeof(*s) - sizeof(s->crc));
  if (crc != s->crc) {
    return -1;
  }

  return 0;
}

int image_set_descriptor_find(const image_set_t *s, uint32_t image_type,
                              const image_descriptor_t **d)
{
  int i;

  for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
    const image_descriptor_t *dd = &s->descriptors[i];
    if (dd->type == image_type) {
      *d = dd;
      return 0;
    }
  }

  return -1;
}
