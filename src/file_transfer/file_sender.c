﻿/**
 * File:   file_sender.c
 * Author: AWTK Develop Team
 * Brief:  file_sender
 *
 * Copyright (c) 2018 - 2019  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2019-11-25 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "tkc/fs.h"
#include "tkc/crc.h"
#include "tkc/mem.h"
#include "tkc/path.h"
#include "tkc/istream.h"
#include "tkc/ostream.h"
#include "tkc/iostream.h"
#include "file_transfer/file_sender.h"

file_sender_t* file_sender_create(const char* filename, tk_iostream_t* io) {
  file_sender_t* sender = NULL;
  return_value_if_fail(filename != NULL && io != NULL, NULL);
  sender = TKMEM_ZALLOC(file_sender_t);
  return_value_if_fail(sender != NULL, NULL);

  sender->io = io;
  sender->filename = filename;
  emitter_init(&(sender->emitter));
  object_ref(OBJECT(io));

  return sender;
}

ret_t file_sender_run(file_sender_t* sender) {
  int32_t ret = 0;
  uint32_t offset = 0;
  int32_t total_size = 0;
  uint32_t timeout = 50000;
  fs_file_t* f = NULL;
  uint8_t* buff = NULL;
  tk_istream_t* in = NULL;
  tk_ostream_t* out = NULL;
  file_transfer_req_t req;
  file_transfer_ack_t ack;
  file_transfer_data_t data;
  file_transfer_header_t header;

  return_value_if_fail(sender != NULL, RET_BAD_PARAMS);

  memset(&req, 0x00, sizeof(req));
  memset(&ack, 0x00, sizeof(ack));
  memset(&data, 0x00, sizeof(data));
  memset(&header, 0x00, sizeof(header));

  total_size = file_get_size(sender->filename);
  goto_error_if_fail(total_size > 0);
  f = fs_open_file(os_fs(), sender->filename, "rb");
  goto_error_if_fail(f != NULL);

  in = tk_iostream_get_istream(sender->io);
  out = tk_iostream_get_ostream(sender->io);
  goto_error_if_fail(in != NULL && out != NULL);

  ret = tk_istream_read_len(in, (uint8_t*)&req, sizeof(req), timeout * 10);
  goto_error_if_fail(ret == sizeof(req));

  goto_error_if_fail(req.block_size <= FILE_TRANSFER_MAX_BLOCK_SIZE);
  buff = TKMEM_ALLOC(req.block_size + 1);
  goto_error_if_fail(buff != NULL);

  data.type = FILE_TRANSFER_PACKET_DATA;
  header.type = FILE_TRANSFER_PACKET_HEADER;
  header.size = total_size;
  path_basename(sender->filename, header.name, sizeof(header.name));

  ret = tk_ostream_write_len(out, (const uint8_t*)&header, sizeof(header), timeout);
  goto_error_if_fail(ret == sizeof(header));

  do {
    ret = fs_file_read(f, buff, req.block_size);
    if (ret <= 0) break;

    data.size = ret;
    data.offset = offset;
    data.crc = tk_crc32(PPPINITFCS32, buff, ret);
    ret = tk_ostream_write_len(out, (const uint8_t*)&data, sizeof(data), timeout);
    goto_error_if_fail(ret == sizeof(data));

    ret = tk_ostream_write_len(out, (const uint8_t*)buff, data.size, timeout);
    goto_error_if_fail(ret == data.size);

    ret = tk_istream_read_len(in, (uint8_t*)&ack, sizeof(ack), timeout);
    goto_error_if_fail(ret == sizeof(ack));
    goto_error_if_fail(ack.type == FILE_TRANSFER_PACKET_ACK);

    offset += data.size;
    log_debug("%u/%u\n", offset, total_size);
    if(offset == total_size) {
      log_debug("transfer done.\n");
      break;
    }
  } while (TRUE);

error:
  TKMEM_FREE(buff);
  if (f != NULL) {
    fs_file_close(f);
  }

  return (offset == total_size && offset > 0) ? RET_OK : RET_FAIL;
}

ret_t file_sender_destroy(file_sender_t* sender) {
  return_value_if_fail(sender != NULL, RET_BAD_PARAMS);

  OBJECT_UNREF(sender->io);
  emitter_deinit(&(sender->emitter));
  TKMEM_FREE(sender);

  return RET_OK;
}
