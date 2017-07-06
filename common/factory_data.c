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
#include "factory_data.h"

int factory_data_header_verify(const factory_data_t *f)
{
  if (le32_to_cpu(f->_signature) != FACTORY_DATA_SIGNATURE) {
    return -1;
  }

  uint32_t header_crc = crc32(0, (const unsigned char *)f,
                              sizeof(*f) - sizeof(f->_header_crc));
  if (le32_to_cpu(f->_header_crc) != header_crc) {
    return -1;
  }

  return 0;
}

int factory_data_body_verify(const factory_data_t *f)
{
  const factory_data_body_t *b = (const factory_data_body_t *)&f->body[0];
  uint32_t body_crc = crc32(0, (const unsigned char *)b,
                            cpu_to_le32(f->_body_size));
  if (le32_to_cpu(f->_body_crc) != body_crc) {
    return -1;
  }

  return 0;
}

int factory_data_populate(factory_data_t *f, const factory_data_params_t *p)
{
  factory_data_body_t *b = (factory_data_body_t *)&f->body[0];

  /* initialize header and body to reserved byte */
  memset((void *)f, FACTORY_DATA_RESERVED_BYTE, sizeof(*f) + sizeof(*b));

  /* populate body */
  b->_hardware =            cpu_to_le32(p->hardware);
  memcpy(b->_mfg_id,        p->mfg_id,        sizeof(b->_mfg_id));
  memcpy(b->_uuid,          p->uuid,          sizeof(b->_uuid));
  b->_timestamp =           cpu_to_le32(p->timestamp);
  memcpy(b->_nap_key,       p->nap_key,       sizeof(b->_nap_key));
  memcpy(b->_mac_address,   p->mac_address,   sizeof(b->_mac_address));
  b->_factory_stage =       cpu_to_le32(p->factory_stage);
  b->_hardware_revision =   cpu_to_le32(p->hardware_revision);
  memcpy(b->_imu_cal,       p->imu_cal,       sizeof(b->_imu_cal));

  /* compute body crc */
  uint32_t body_crc = crc32(0, (const unsigned char *)b, sizeof(*b));

  /* populate header */
  f->_signature =   cpu_to_le32(FACTORY_DATA_SIGNATURE);
  f->_body_size =   cpu_to_le32(sizeof(*b));
  f->_body_crc =    cpu_to_le32(body_crc);

  /* compute header crc */
  uint32_t header_crc = crc32(0, (const unsigned char *)f,
                              sizeof(*f) - sizeof(f->_header_crc));

  /* populate header crc */
  f->_header_crc =  cpu_to_le32(header_crc);

  return 0;
}
