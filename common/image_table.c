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

#ifdef USE_HOSTCC
#include <u-boot/crc.h>
#else
#include <common.h>
#endif /* !USE_HOSTCC*/
#include "image_table.h"

int image_set_verify(const image_set_t *s)
{
  if (s->_signature != cpu_to_le32(IMAGE_SET_SIGNATURE)) {
    return -1;
  }

  uint32_t crc = crc32(0, (const unsigned char *)s,
                       sizeof(*s) - sizeof(s->_crc));
  if (s->_crc != cpu_to_le32(crc)) {
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
    if (dd->_type == cpu_to_le32(image_type)) {
      *d = dd;
      return 0;
    }
  }

  return -1;
}

void image_set_init(image_set_t *s, const image_set_params_t *p)
{
  memset((void *)s, IMAGE_SET_RESERVED_BYTE, sizeof(image_set_t));
  s->_signature =   cpu_to_le32(IMAGE_SET_SIGNATURE);
  s->_version =     cpu_to_le32(p->version);
  s->_timestamp =   cpu_to_le32(p->timestamp);
  s->_seq_num =     cpu_to_le32(p->seq_num);
  s->_hardware =    cpu_to_le32(p->hardware);
  memcpy(s->_name, p->name, sizeof(s->_name));
}

int image_set_descriptor_add(image_set_t *s,
                             const image_descriptor_params_t *p)
{
  int i;

  for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
    image_descriptor_t *d = &s->descriptors[i];
    if (d->_type == cpu_to_le32(IMAGE_TYPE_INVALID)) {

      d->_type =            cpu_to_le32(p->type);
      d->_version =         cpu_to_le32(p->version);
      d->_timestamp =       cpu_to_le32(p->timestamp);
      d->_load_address =    cpu_to_le32(p->load_address);
      d->_entry_address =   cpu_to_le32(p->entry_address);
      d->_data_offset =     cpu_to_le32(p->data_offset);
      d->_data_size =       cpu_to_le32(p->data_size);
      d->_data_crc =        cpu_to_le32(p->data_crc);
      memcpy(d->_name, p->name, sizeof(d->_name));

      return 0;
    }
  }

  return -1;
}

void image_set_finalize(image_set_t *s)
{
  uint32_t crc = crc32(0, (const unsigned char *)s,
                       sizeof(*s) - sizeof(s->_crc));
  s->_crc = cpu_to_le32(crc);
}

void image_descriptor_data_crc_init(uint32_t *crc)
{
  *crc = 0;
}

void image_descriptor_data_crc_continue(uint32_t *crc, const uint8_t *data,
                                        uint32_t data_length)
{
  *crc = crc32(*crc, data, data_length);
}
