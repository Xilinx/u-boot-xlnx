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

#include <factory_data.h>
#include <image_table.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const char *cmd = NULL;

static struct {
  const char *output_filename;
  const char *verify_filename;
  bool print;
  factory_data_params_t factory_data_params;
} args = {
  .output_filename = NULL,
  .verify_filename = NULL,
  .print = false,
  .factory_data_params = {
    .hardware = 0,
    .mfg_serial_number = {0},
    .uuid = {0},
    .timestamp = 0,
    .nap_key = {0},
    .mac_address = {0},
    .factory_stage = 0,
  }
};

static const struct {
  uint32_t hardware;
  const char *name;
} image_hardware_strings[] = {
  { IMAGE_HARDWARE_UNKNOWN,     "unknown"   },
  { IMAGE_HARDWARE_MICROZED,    "microzed"  },
  { IMAGE_HARDWARE_EVT1,        "evt1"      },
  { IMAGE_HARDWARE_EVT2,        "evt2"      },
  { IMAGE_HARDWARE_DVT,         "dvt"       },
};

static void usage(void)
{
  uint32_t i;

  printf("Usage: %s\n", cmd);

  puts("\nOutput - specify factory data output parameters");
  puts("\t-o, --out <file>");
  puts("\t\toutput file");
  puts("\t-h, --hardware <hardware>");
  printf("\t\tstring: ");
  for (i=0; i<ARRAY_SIZE(image_hardware_strings); i++) {
    printf("%s ", image_hardware_strings[i].name);
  }
  printf("\n");
  puts("\t-s, --serial-number <serial-number>");
  puts("\t\tserial number");
  puts("\t-t, --timestamp <timestamp>");
  puts("\t\ttimestamp");
  puts("\t-k, --nap-key <nap-key>");
  puts("\t\tnap key, 16B hex string");
  puts("\t-m, --mac-address <mac-address>");
  puts("\t\tmac address, 6B hex string");

  puts("\nMisc options");
  puts("\t--verify <file>");
  puts("\t\tfile to verify");
  puts("\t-p, --print");
  puts("\t\tprint information when finished");
}

static char nibble_to_char(uint8_t nibble)
{
  if (nibble < 10) {
    return '0' + nibble;
  } else if (nibble < 16) {
    return 'a' + nibble - 10;
  } else {
    return '?';
  }
}

static void print_hex_string(char *str, const uint8_t *data, uint32_t data_size)
{
  int i;
  for (i=0; i<data_size; i++) {
    str[2*i + 0] = nibble_to_char((data[data_size - 1 - i] >> 4) & 0xf);
    str[2*i + 1] = nibble_to_char((data[data_size - 1 - i] >> 0) & 0xf);
  }
  str[2*i] = 0;
}

static int parse_hex_string(const char *str, uint8_t *data, uint32_t data_size)
{
  /* skip leading "0x" if present */
  int str_offset = 0;
  if (strncasecmp(str, "0x", 2) == 0) {
    str_offset += 2;
  }

  int i;
  for (i=0; i<2*data_size; i++) {

    char c = str[str_offset + i];
    if (c == 0) {
      return -1;
    }

    uint8_t nibble;
    if ('0' <= c && c <= '9') {
      nibble = c - '0';
    } else if ('a' <= c && c <= 'f') {
      nibble = c - 'a' + 10;
    } else {
      return -1;
    }

    if (i % 2 == 0) {
      data[data_size - 1 - i/2] = nibble << 4;
    } else {
      data[data_size - 1 - i/2] |= nibble;
    }
  }

  /* verify that the entire string was used */
  if (str[str_offset + i] != 0) {
    return -1;
  }

  return 0;
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_VERIFY = 1
  };

  const struct option long_opts[] = {
    {"out",               required_argument, 0, 'o'},
    {"hardware",          required_argument, 0, 'h'},
    {"mfg-serial-number", required_argument, 0, 's'},
    {"uuid",              required_argument, 0, 'u'},
    {"timestamp",         required_argument, 0, 't'},
    {"nap-key",           required_argument, 0, 'k'},
    {"mac-address",       required_argument, 0, 'm'},
    {"factory-stage",     required_argument, 0, 'f'},
    {"verify",            required_argument, 0, OPT_ID_VERIFY},
    {"print",             no_argument,       0, 'p'},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "o:h:s:t:k:m:p",
                          long_opts, &opt_index)) != -1) {
    switch (c) {
      case 'o': {
        args.output_filename = optarg;
      }
      break;

      case 'h': {
        bool found = false;
        uint32_t i;
        for (i=0; i<ARRAY_SIZE(image_hardware_strings); i++) {
          if (strcasecmp(optarg, image_hardware_strings[i].name) == 0) {
            args.factory_data_params.hardware =
                image_hardware_strings[i].hardware;
            found = true;
            break;
          }
        }

        if (!found) {
          fprintf(stderr, "invalid image hardware: \"%s\"\n", optarg);
          return -1;
        }
      }
      break;

      case 's': {
        strncpy((char *)args.factory_data_params.mfg_serial_number, optarg,
            sizeof(args.factory_data_params.mfg_serial_number));
      }
      break;

      case 'u': {
         if (parse_hex_string(optarg, args.factory_data_params.uuid,
                             sizeof(args.factory_data_params.uuid)) != 0) {
          fprintf(stderr, "invalid uuid: \"%s\"\n", optarg);
          return -1;
        }
      }
      break;

      case 't': {
        args.factory_data_params.timestamp = strtol(optarg, NULL, 0);
      }
      break;

      case 'k': {
        if (parse_hex_string(optarg, args.factory_data_params.nap_key,
                             sizeof(args.factory_data_params.nap_key)) != 0) {
          fprintf(stderr, "invalid nap key: \"%s\"\n", optarg);
          return -1;
        }
      }
      break;

      case 'm': {
        if (parse_hex_string(optarg, args.factory_data_params.mac_address,
                             sizeof(args.factory_data_params.mac_address))
                                 != 0) {
          fprintf(stderr, "invalid mac address: \"%s\"\n", optarg);
          return -1;
        }
      }
      break;

      case 'f': {
        args.factory_data_params.factory_stage = strtol(optarg, NULL, 0);
      }
      break;

      case OPT_ID_VERIFY: {
        args.verify_filename = optarg;
      }
      break;

      case 'p': {
        args.print = true;
      }
      break;

      default: {
        fprintf(stderr, "invalid option\n");
        return -1;
      }
      break;
    }
  }

  if ((args.output_filename == NULL) && (args.verify_filename == NULL)) {
    fprintf(stderr, "output file or verify file must be specified\n");
    return -1;
  }

  return 0;
}

static void factory_data_print(const factory_data_t *f)
{
  uint32_t hardware;
  if (factory_data_hardware_get(f, &hardware) == 0) {
    printf("Hardware:       %08x\n", hardware);
  }

  uint8_t mfg_id[sizeof(args.factory_data_params.mfg_serial_number)];
  if (factory_data_mfg_serial_number_get(f, mfg_id) == 0) {
    char mfg_id_string[sizeof(mfg_id) + 1];
    memcpy(mfg_id_string, mfg_id, sizeof(mfg_id));
    mfg_id_string[sizeof(mfg_id)] = 0;
    printf("Mfg ID:         %s\n", mfg_id_string);
  }

  uint8_t uuid[sizeof(args.factory_data_params.uuid)];
  if (factory_data_uuid_get(f, uuid) == 0) {
    char uuid_string[2 * sizeof(uuid) + 1];
    print_hex_string(uuid_string, uuid, sizeof(uuid));
    printf("UUID:           %s\n", uuid_string);
  }

  uint32_t timestamp;
  if (factory_data_timestamp_get(f, &timestamp) == 0) {
    printf("Timestamp:      %08x\n", timestamp);
  }

  uint8_t nap_key[16];
  if (factory_data_nap_key_get(f, nap_key) == 0) {
    char nap_key_string[2 * sizeof(nap_key) + 1];
    print_hex_string(nap_key_string, nap_key, sizeof(nap_key));
    printf("NAP Key:        %s\n", nap_key_string);
  }

  uint8_t mac_address[6];
  if (factory_data_mac_address_get(f, mac_address) == 0) {
    char mac_address_string[2 * sizeof(mac_address) + 1];
    print_hex_string(mac_address_string, mac_address, sizeof(mac_address));
    printf("MAC Address:    %s\n", mac_address_string);
  }

  uint32_t factory_stage;
  if (factory_data_timestamp_get(f, &factory_stage) == 0) {
    printf("Factory Stage:  %08x\n", factory_stage);
  }
}

static int do_output(void)
{
  /* open output file */
  int fd = open(args.output_filename,
                O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
  if (fd < 0) {
    fprintf(stderr, "%s: Can't open %s: %s\n",
            cmd, args.output_filename, strerror(errno));
    return -1;
  }

  uint8_t factory_data_buff[sizeof(factory_data_t) +
                            sizeof(factory_data_body_t)];
  factory_data_t *factory_data = (factory_data_t *)factory_data_buff;
  if (factory_data_populate(factory_data, &args.factory_data_params) != 0) {
    fprintf(stderr, "%s: Error populating factory data\n",
            cmd);
    return -1;
  }

  /* write to file */
  if (write(fd, (void *)factory_data_buff, sizeof(factory_data_buff))
          != sizeof(factory_data_buff)) {
    fprintf(stderr, "%s: Write error on %s: %s\n",
            cmd, args.output_filename, strerror(errno));

    return -1;
  }

  /* close output file */
  fsync(fd);
  if (close(fd)) {
    fprintf(stderr, "%s: Write error on %s: %s\n",
            cmd, args.output_filename, strerror(errno));
    return -1;
  }

  /* print information if requested */
  if (args.print) {
    factory_data_print(factory_data);
  }

  return 0;
}

static int do_verify(void)
{
  /* open verify file */
  int fd = open(args.verify_filename, O_RDONLY | O_BINARY);
  if (fd < 0) {
    fprintf(stderr, "%s: Can't open %s: %s\n",
            cmd, args.verify_filename, strerror(errno));
    return -1;
  }

  /* stat file */
  struct stat sbuf;
  if (fstat(fd, &sbuf) < 0) {
    fprintf(stderr, "%s: Can't stat %s: %s\n",
            cmd, args.verify_filename, strerror(errno));
    return -1;
  }
  uint32_t filesize = sbuf.st_size;

  /* make sure there is room for the header */
  if (filesize < sizeof(factory_data_t)) {
    fprintf(stderr, "%s: Bad size: \"%s\" is not a valid factory data image\n",
            cmd, args.verify_filename);
    return -1;
  }

  /* mmap file */
  const uint8_t *ptr = mmap(0, filesize, PROT_READ, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    fprintf(stderr, "%s: Can't read %s: %s\n",
            cmd, args.verify_filename, strerror(errno));
    return -1;
  }

  /* verify header */
  const factory_data_t *factory_data = (const factory_data_t *)ptr;
  printf("Verifying factory data header: ");
  if (factory_data_header_verify(factory_data) != 0) {
    printf("FAILED\n");
    return -1;
  }
  printf("OK\n");

  /* verify body */
  printf("Verifying factory data body: ");
  if (factory_data_body_verify(factory_data) != 0) {
    printf("FAILED\n");
    return -1;
  }
  printf("OK\n");

  printf("Verification successful\n");

  /* print information if requested */
  if (args.print) {
    factory_data_print(factory_data);
  }

  /* close file */
  munmap((void *)ptr, filesize);
  fsync(fd);
  if (close(fd)) {
    fprintf(stderr, "%s: Write error on %s: %s\n",
            cmd, args.verify_filename, strerror(errno));
    return -1;
  }

  return 0;
}

int main(int argc, char **argv)
{
  cmd = basename(argv[0]);

  if (parse_options(argc, argv) != 0) {
    usage();
    exit(EXIT_FAILURE);
  }

  if (args.output_filename != NULL) {
    if (do_output() != 0) {
      exit(EXIT_FAILURE);
    }
  }

  if (args.verify_filename != NULL) {
    if (do_verify() != 0) {
      exit(EXIT_FAILURE);
    }
  }

  exit(EXIT_SUCCESS);
}

