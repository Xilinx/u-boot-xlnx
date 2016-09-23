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

#include <image_table.h>
#include <imagetool.h>
#include <image.h>
#include <getopt.h>

static const char *cmd = NULL;

typedef struct {
  const char *filename;
  uint32_t type;
  uint32_t version;
  uint32_t data_offset;
} image_file_params_t;

typedef struct {
  int fd;
  uint32_t *offset;
} image_append_params_t;

static struct {
  const char *output_filename;
  bool append;
  image_file_params_t image_file_params[IMAGE_SET_DESCRIPTORS_COUNT];
  uint32_t image_file_params_count;
  image_set_params_t image_set_params;
  const char *verify_filename;
  bool print;
  bool print_images;
} args = {
  .output_filename = NULL,
  .append = false,
  .image_file_params_count = 0,
  .image_set_params = {
    .version = 0,
    .timestamp = 0,
    .seq_num = 0,
    .hardware = 0,
    .name = {0}
  },
  .verify_filename = NULL,
  .print = false,
  .print_images = false
};

static const struct {
  uint32_t type;
  const char *name;
} image_type_strings[] = {
  { IMAGE_TYPE_UNKNOWN,     "unknown"   },
  { IMAGE_TYPE_LOADER,      "loader"    },
  { IMAGE_TYPE_UBOOT_SPL,   "uboot-spl" },
  { IMAGE_TYPE_UBOOT,       "uboot"     },
  { IMAGE_TYPE_LINUX,       "linux"     },
};

static const struct {
  uint32_t hardware;
  const char *name;
} image_hardware_strings[] = {
  { IMAGE_HARDWARE_UNKNOWN,     "unknown"   },
  { IMAGE_HARDWARE_MICROZED,    "microzed"  },
  { IMAGE_HARDWARE_EVT1,        "evt1"      },
  { IMAGE_HARDWARE_EVT2,        "evt2"      },
};

static void usage(void)
{
  uint32_t i;

  printf("Usage: %s\n", cmd);

  puts("\nOutput - specify image set output parameters");
  puts("\t-o, --out <file>");
  puts("\t\toutput file");
  puts("\t-a, --append");
  puts("\t\tappend image data to output file, ignoring --image-offset");
  puts("\t-v, --version <version>");
  puts("\t\tpacked 32-bit version");
  puts("\t-t, --timestamp <timestamp>");
  puts("\t\ttimestamp");
  puts("\t-s, --seq-num <seq-num>");
  puts("\t\tsequence number");
  puts("\t-h, --hardware <hardware>");
  printf("\t\tstring: ");
  for (i=0; i<ARRAY_SIZE(image_hardware_strings); i++) {
    printf("%s ", image_hardware_strings[i].name);
  }
  printf("\n");
  puts("\t-n, --name <name>");
  puts("\t\tname");

  puts("\nImages - specify image parameters. Multiple images may be added");
  puts("in groups, each starting with --image");
  puts("\t--image <file>");
  puts("\t\timage source file");
  puts("\t--image-type <type>");
  printf("\t\tstring: ");
  for (i=0; i<ARRAY_SIZE(image_type_strings); i++) {
    printf("%s ", image_type_strings[i].name);
  }
  printf("\n");
  puts("\t--image-version <version>");
  puts("\t\tpacked 32-bit version");
  puts("\t--image-offset <offset>");
  puts("\t\toffset of image data into file");

  puts("\nMisc options");
  puts("\t--verify <file>");
  puts("\t\tfile to verify");
  puts("\t-p, --print");
  puts("\t\tprint information when finished");
  puts("\t--print-images");
  puts("\t\tprint input image information");
}

static int parse_options(int argc, char *argv[])
{
  uint32_t image_file_params_index = 0;

  enum {
    OPT_ID_IMAGE = 1,
    OPT_ID_IMAGE_TYPE,
    OPT_ID_IMAGE_VERSION,
    OPT_ID_IMAGE_OFFSET,
    OPT_ID_VERIFY,
    OPT_ID_PRINT_IMAGES
  };

  const struct option long_opts[] = {
    {"out",             required_argument, 0, 'o'},
    {"append",          no_argument,       0, 'a'},
    {"version",         required_argument, 0, 'v'},
    {"timestamp",       required_argument, 0, 't'},
    {"seq-num",         required_argument, 0, 's'},
    {"hardware",        required_argument, 0, 'h'},
    {"name",            required_argument, 0, 'n'},
    {"image",           required_argument, 0, OPT_ID_IMAGE},
    {"image-type",      required_argument, 0, OPT_ID_IMAGE_TYPE},
    {"image-version",   required_argument, 0, OPT_ID_IMAGE_VERSION},
    {"image-offset",    required_argument, 0, OPT_ID_IMAGE_OFFSET},
    {"verify",          required_argument, 0, OPT_ID_VERIFY},
    {"print",           no_argument,       0, 'p'},
    {"print-images",    no_argument,       0, OPT_ID_PRINT_IMAGES},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "o:av:t:s:h:n:p",
                          long_opts, &opt_index)) != -1) {
    switch (c) {
      case 'o': {
        args.output_filename = optarg;
      }
      break;

      case 'a': {
        args.append = true;
      }
      break;

      case 'v': {
        args.image_set_params.version = strtol(optarg, NULL, 0);
      }
      break;

      case 't': {
        args.image_set_params.timestamp = strtol(optarg, NULL, 0);
      }
      break;

      case 's': {
        args.image_set_params.seq_num = strtol(optarg, NULL, 0);
      }
      break;

      case 'h': {
        bool found = false;
        uint32_t i;
        for (i=0; i<ARRAY_SIZE(image_hardware_strings); i++) {
          if (strcasecmp(optarg, image_hardware_strings[i].name) == 0) {
            args.image_set_params.hardware =
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

      case 'n': {
        strncpy((char *)args.image_set_params.name, optarg,
                sizeof(args.image_set_params.name));
      }
      break;

      case OPT_ID_IMAGE: {
        /* increment index if this is not the first image */
        if (args.image_file_params_count > 0) {
          image_file_params_index++;
        }

        if (args.image_file_params_count < IMAGE_SET_DESCRIPTORS_COUNT) {
          args.image_file_params_count++;
        } else {
          fprintf(stderr, "too many image files\n");
          return -1;
        }

        /* initialize params and set filename */
        memset((void *)&args.image_file_params[image_file_params_index], 0,
               sizeof(image_file_params_t));
        args.image_file_params[image_file_params_index].filename = optarg;
      }
      break;

      case OPT_ID_IMAGE_TYPE: {
        bool found = false;
        uint32_t i;
        for (i=0; i<ARRAY_SIZE(image_type_strings); i++) {
          if (strcasecmp(optarg, image_type_strings[i].name) == 0) {
            args.image_file_params[image_file_params_index].type =
                image_type_strings[i].type;
            found = true;
            break;
          }
        }

        if (!found) {
          fprintf(stderr, "invalid image type: \"%s\"\n", optarg);
          return -1;
        }
      }
      break;

      case OPT_ID_IMAGE_VERSION: {
        args.image_file_params[image_file_params_index].version =
            strtol(optarg, NULL, 0);
      }
      break;

      case OPT_ID_IMAGE_OFFSET: {
        args.image_file_params[image_file_params_index].data_offset =
            strtol(optarg, NULL, 0);
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

      case OPT_ID_PRINT_IMAGES: {
        args.print_images = true;
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

  if (args.append) {
    uint32_t i;
    for (i=0; i<ARRAY_SIZE(image_type_strings); i++) {
      if (args.image_file_params[i].data_offset != 0) {
        fprintf(stderr, "image offset specified with --append\n");
        return -1;
      }
    }
  }

  return 0;
}

static void image_set_print(const image_set_t *s)
{
  uint8_t name[33];

  image_set_name_get(s, name);
  name[sizeof(name)-1] = 0;
  printf("Image set header:\n");
  printf("Name:         %s\n",   name);
  printf("Version:      %08x\n", image_set_version_get(s));
  printf("Timestamp:    %08x\n", image_set_timestamp_get(s));
  printf("Hardware:     %08x\n", image_set_hardware_get(s));
  printf("Seq Num:      %08x\n", image_set_seq_num_get(s));

  uint32_t i;
  for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
    const image_descriptor_t *d = &s->descriptors[i];
    if (image_descriptor_type_get(d) != IMAGE_TYPE_INVALID) {
      image_descriptor_name_get(d, name);
      name[sizeof(name)-1] = 0;
      printf("Image descriptor %u:\n", i);
      printf("Type:         %08x\n", image_descriptor_type_get(d));
      printf("Name:         %s\n",   name);
      printf("Version:      %08x\n", image_descriptor_version_get(d));
      printf("Timestamp:    %08x\n", image_descriptor_timestamp_get(d));
      printf("Load Addr:    %08x\n", image_descriptor_load_address_get(d));
      printf("Entry Addr:   %08x\n", image_descriptor_entry_address_get(d));
      printf("Data Offset:  %08x\n", image_descriptor_data_offset_get(d));
      printf("Data Size:    %08x\n", image_descriptor_data_size_get(d));
      printf("Data CRC:     %08x\n", image_descriptor_data_crc_get(d));
    }
  }
}

static int image_file_append(image_set_t *image_set,
                             const image_file_params_t *file_params,
                             const image_append_params_t *append_params)
{
  /* open file */
  int fd = open(file_params->filename, O_RDONLY | O_BINARY);
  if (fd < 0) {
    fprintf(stderr, "%s: Can't open %s: %s\n",
            cmd, file_params->filename, strerror(errno));
    return -1;
  }

  /* stat file */
  struct stat sbuf;
  if (fstat(fd, &sbuf) < 0) {
    fprintf(stderr, "%s: Can't stat %s: %s\n",
            cmd, file_params->filename, strerror(errno));
    return -1;
  }
  uint32_t filesize = sbuf.st_size;

  /* make sure there is room for the header */
  if (filesize < image_get_header_size()) {
    fprintf(stderr, "%s: Bad size: \"%s\" is not a valid image\n",
            cmd, file_params->filename);
    return -1;
  }

  /* mmap file */
  const uint8_t *ptr = mmap(0, filesize, PROT_READ, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    fprintf(stderr, "%s: Can't read %s: %s\n",
            cmd, file_params->filename, strerror(errno));
    return -1;
  }

  /* get header info */
  const image_header_t *header = (const image_header_t *)ptr;
  uint8_t image_type = image_get_type(header);
  struct image_type_params *image_type_params = imagetool_get_type(image_type);
  struct image_tool_params image_tool_params = {
    .cmdname = (char *)cmd,
    .imagefile = (char *)file_params->filename
  };

  /* verify header */
  if ((image_type_params->verify_header == NULL) ||
      (image_type_params->verify_header((void *)ptr, filesize,
                                        &image_tool_params) != 0)) {
    fprintf(stderr, "%s: Error verifying header for %s\n",
            cmd, file_params->filename);
    return -1;
  }

  /* print header if requested*/
  if (args.print_images) {
    if (image_type_params->print_header == NULL) {
      fprintf(stderr, "%s: Unable to print header for %s\n",
              cmd, file_params->filename);
    } else {
      image_type_params->print_header(ptr);
    }
  }

  /* compute data crc */
  uint32_t data_crc;
  image_descriptor_data_crc_init(&data_crc);
  image_descriptor_data_crc_continue(&data_crc, ptr, filesize);

  /* append image descriptor to image set */
  image_descriptor_params_t image_descriptor_params = {
    .type =             file_params->type,
    .version =          file_params->version,
    .timestamp =        image_get_time(header),
    .load_address =     image_get_load(header),
    .entry_address =    image_get_ep(header),
    .data_offset =      (append_params != NULL) ? *append_params->offset :
                                                  file_params->data_offset,
    .data_size =        filesize,
    .data_crc =         data_crc
  };
  strncpy((char *)image_descriptor_params.name, image_get_name(header),
          sizeof(image_descriptor_params.name));
  if (image_set_descriptor_add(image_set, &image_descriptor_params) != 0) {
    fprintf(stderr, "%s: Error adding descriptor for %s\n",
            cmd, file_params->filename);
    return -1;
  }

  /* append data to output file if requested */
  if (append_params != NULL) {
    if (write(append_params->fd, (void *)ptr, filesize) != filesize) {
      fprintf(stderr, "%s: Error appending data to output file: %s\n",
              cmd, strerror(errno));

      return -1;
    }
    *append_params->offset += filesize;
  }

  /* close file */
  munmap((void *)ptr, filesize);
  fsync(fd);
  if (close(fd)) {
    fprintf(stderr, "%s: Write error on %s: %s\n",
            cmd, file_params->filename, strerror(errno));
    return -1;
  }

  return 0;
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

  /* leave space for image set header */
  if (lseek(fd, sizeof(image_set_t), SEEK_SET) != sizeof(image_set_t)) {
    fprintf(stderr, "%s: Error seeking %s: %s\n",
            cmd, args.output_filename, strerror(errno));
    return -1;
  }

  /* build image set, appending image data if required */
  image_set_t image_set;
  image_set_init(&image_set, &args.image_set_params);

  uint32_t append_offset = sizeof(image_set_t);
  image_append_params_t image_append_params = {
    .fd = fd,
    .offset = &append_offset
  };
  image_append_params_t *p_image_append_params =
      args.append ? &image_append_params : NULL;

  uint32_t i;
  for (i=0; i<args.image_file_params_count; i++) {
    if (image_file_append(&image_set, &args.image_file_params[i],
                          p_image_append_params) != 0) {
      return -1;
    }
  }

  image_set_finalize(&image_set);

  /* seek back to start of file */
  if (lseek(fd, 0, SEEK_SET) != 0) {
    fprintf(stderr, "%s: Error seeking %s: %s\n",
            cmd, args.output_filename, strerror(errno));
    return -1;
  }

  /* write image set header to file */
  if (write(fd, (void *)&image_set, sizeof(image_set)) != sizeof(image_set)) {
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
    image_set_print(&image_set);
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

  /* make sure there is room for the image set header */
  if (filesize < sizeof(image_set_t)) {
    fprintf(stderr, "%s: Bad size: \"%s\" is not a valid image set\n",
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

  /* verify image set header */
  const image_set_t *image_set = (const image_set_t *)ptr;
  printf("Verifying image set header: ");
  if (image_set_verify(image_set) != 0) {
    printf("FAILED\n");
    return -1;
  }
  printf("OK\n");

  /* verify image data if present */
  if (filesize > sizeof(image_set_t)) {
    uint32_t i;
    for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
      const image_descriptor_t *d = &image_set->descriptors[i];
      if (image_descriptor_type_get(d) != IMAGE_TYPE_INVALID) {
        printf("Verifying component image %u: ", i);

        uint32_t data_offset = image_descriptor_data_offset_get(d);
        uint32_t data_size = image_descriptor_data_size_get(d);

        /* make sure there is room for the image data */
        if ((data_offset >= filesize) ||
            (data_size > filesize - data_offset)) {
          printf("FAILED\n");
          return -1;
        }

        /* verify data CRC */
        const uint8_t *data = &ptr[data_offset];
        uint32_t computed_data_crc;
        image_descriptor_data_crc_init(&computed_data_crc);
        image_descriptor_data_crc_continue(&computed_data_crc, data, data_size);

        if (image_descriptor_data_crc_get(d) != computed_data_crc) {
          printf("FAILED\n");
          return -1;
        }
        printf("OK\n");
      }
    }
  }

  printf("Verification successful\n");

  /* print information if requested */
  if (args.print) {
    image_set_print(image_set);
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

