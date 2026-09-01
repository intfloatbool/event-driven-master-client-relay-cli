#pragma once

#include <enet/enet.h>
#include <event.h>
#include <msgpack.h>
#include <stdint.h>
#include <string.h>

#include "ifb_enet_safe.h"

const uint16_t PTCL_PORT = 7777;
const size_t ERR_BUFF_SIZE = 128;
const uint8_t IFB_CNST_NET_CHANNELS_COUNT = 3;

typedef enum {
  IFB_NET_CHANNEL_CRITICAL = 0,
  IFB_NET_CHANNEL_STREAM = 1,
  IFB_NET_CHANNEL_SERVICE = 2
} ifb_en_channel_id;

typedef enum {
  IFB_MSG_TYPE_NONE = 0,
  IFB_MSG_TYPE_REQ_USER_INFO = 1,
  IFB_MSG_TYPE_RES_USER_INFO = 2,
  IFB_MSG_TYPE_GAME_STATE_UPD = 3,
  IFB_MSG_TYPE_PLAYER_INPUT = 4,
  IFB_MSG_TYPE_SLAVE_CONNECTED = 5
} ifb_en_message_type;

typedef struct ifb_strct_player_input {
  uint32_t id;
  int32_t x;
  int32_t y;
} ifb_strct_player_input;

typedef struct ifb_strct_user_info {
  uint32_t id;
  bool is_master;
} ifb_strct_user_info;

typedef struct ifb_strct_player_pos {
  bool is_exists;
  int32_t x;
  int32_t y;
} ifb_strct_player_pos;

typedef struct ifb_strct_game_state {
  uint32_t tick;
  ifb_strct_player_pos pos_1;
  ifb_strct_player_pos pos_2;
  ifb_strct_player_pos pos_3;
  ifb_strct_player_pos pos_4;
} ifb_strct_game_state;

bool ifb_ptcl_try_send_msg_packet(ifb_en_message_type msg_type,
                                  ifb_en_channel_id channel_id,
                                  msgpack_sbuffer *sbuf, ENetPeer *peer,
                                  ENetHost *host, bool is_reliable,
                                  bool is_forced, char *error_buffer,
                                  size_t error_buf_size) {

  switch (msg_type) {
  case IFB_MSG_TYPE_REQ_USER_INFO: {
    break;
  }
  case IFB_MSG_TYPE_RES_USER_INFO: {
    break;
  }
  case IFB_MSG_TYPE_GAME_STATE_UPD: {
    break;
  }
  case IFB_MSG_TYPE_SLAVE_CONNECTED: {
    break;
  }
  case IFB_MSG_TYPE_PLAYER_INPUT: {
    break;
  }
  default: {
    snprintf(error_buffer, error_buf_size,
             "unknown ifb_en_message_type msg_type!");
    return false;
  }
  }

  uint8_t *data = NULL;
  size_t data_size = 0;

  if (sbuf != NULL) {
    data_size = sizeof(uint8_t) + sbuf->size;
    data = (uint8_t *)malloc(data_size);
    if (data == NULL) {
      snprintf(error_buffer, error_buf_size, "malloc() failed.");
      return false;
    }

    data[0] = msg_type;
    memcpy(data + 1, sbuf->data, sbuf->size);
  } else {
    data_size = sizeof(uint8_t);
    data = (uint8_t *)malloc(data_size);
    if (data == NULL) {
      snprintf(error_buffer, error_buf_size, "malloc() failed.");
      return false;
    }
    data[0] = msg_type;
  }

  ENetPacket *pkt =
      enet_packet_create(data, data_size,
                         is_reliable ? ENET_PACKET_FLAG_RELIABLE
                                     : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
  if (!try_enet_peer_send(peer, channel_id, pkt, error_buffer,
                          error_buf_size)) {
    enet_packet_destroy(pkt);
    free(data);
    return false;
  }

  if (is_forced) {
    enet_host_flush(host);
  }

  free(data);
  return true;
}

bool ifb_try_relay_packet(ENetPacket *pkt_src, ENetPeer *peer_to,
                          ifb_en_channel_id channel_id, bool is_reliable,
                          char *err_buf, size_t err_buf_size) {
  ENetPacket *pkt =
      enet_packet_create(pkt_src->data, pkt_src->dataLength,
                         is_reliable ? ENET_PACKET_FLAG_RELIABLE
                                     : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);

  int send_res = enet_peer_send(peer_to, channel_id, pkt);

  if (send_res != 0) {
    snprintf(err_buf, err_buf_size, "enet_peer_send failed.");
    enet_packet_destroy(pkt);
    return false;
  }

  return true;
}

const char *ifb_msg_type_to_str(ifb_en_message_type msg_type) {
  switch (msg_type) {
  case (IFB_MSG_TYPE_REQ_USER_INFO): {
    return "IFB_MSG_TYPE_REQ_USER_INFO";
  }
  case (IFB_MSG_TYPE_RES_USER_INFO): {
    return "IFB_MSG_TYPE_RES_USER_INFO";
  }
  case (IFB_MSG_TYPE_GAME_STATE_UPD): {
    return "IFB_MSG_TYPE_GAME_STATE_UPD";
  }
  case (IFB_MSG_TYPE_SLAVE_CONNECTED): {
    return "IFB_MSG_TYPE_SLAVE_CONNECTED";
  }
  case (IFB_MSG_TYPE_PLAYER_INPUT): {
    return "IFB_MSG_TYPE_PLAYER_INPUT";
  }
  default: {
    return "UNKNOWN";
  }
  }
}

bool ifb_ptcl_try_parse_message_by_type(ENetEvent *event,
                                        ifb_en_message_type *out_msg_type,
                                        uint8_t **out_only_msg_payload,
                                        size_t *out_only_msg_payload_size,
                                        char *err_buf, size_t err_buf_size) {

  size_t payload_size = event->packet->dataLength;

  if (payload_size < 1) {
    return false;
  }

  const uint8_t *payload = (const uint8_t *)event->packet->data;

  bool res = false;
  uint8_t msg_type = payload[0];
  switch (msg_type) {
  case IFB_MSG_TYPE_REQ_USER_INFO: {
    res = true;
    *out_msg_type = (ifb_en_message_type)msg_type;
    break;
  }
  case IFB_MSG_TYPE_RES_USER_INFO: {
    res = true;
    *out_msg_type = (ifb_en_message_type)msg_type;
    break;
  }
  case IFB_MSG_TYPE_GAME_STATE_UPD: {
    res = true;
    *out_msg_type = (ifb_en_message_type)msg_type;
    break;
  }
  case IFB_MSG_TYPE_SLAVE_CONNECTED: {
    res = true;
    *out_msg_type = (ifb_en_message_type)msg_type;
    break;
  }
  case IFB_MSG_TYPE_PLAYER_INPUT: {
    res = true;
    *out_msg_type = (ifb_en_message_type)msg_type;
    break;
  }
  default: {
    snprintf(err_buf, err_buf_size, "unknown msg_type: %u", msg_type);
    res = false;
    break;
  }
  }

  if (payload_size > 1) {
    *out_only_msg_payload = (uint8_t *)(event->packet->data + 1);
    *out_only_msg_payload_size = payload_size - 1;
  }

  return res;
}

bool ifb_try_pack_user_info(msgpack_sbuffer *sbuf, ifb_strct_user_info *user,
                            char *err_buf, size_t err_buf_size) {

  msgpack_packer pk;
  msgpack_packer_init(&pk, sbuf, msgpack_sbuffer_write);

  // 1. id
  // 2. is_master
  size_t keys_count = 2;
  if (msgpack_pack_map(&pk, keys_count) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_map failed.");
    return false;
  }

  // key
  const char *id_key = "id";
  if (msgpack_pack_str(&pk, strlen(id_key)) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_str 'id' KEY failed.");
    return false;
  }
  if (msgpack_pack_str_body(&pk, id_key, strlen(id_key)) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_str_body 'id' KEY failed.");
    return false;
  }
  // value
  if (msgpack_pack_uint32(&pk, user->id) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_uint32 'id' VALUE failed.");
    return false;
  }

  // key
  const char *is_master_key = "is_master";
  if (msgpack_pack_str(&pk, strlen(is_master_key)) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_str 'is_master' KEY failed.");
    return false;
  }
  if (msgpack_pack_str_body(&pk, is_master_key, strlen(is_master_key)) != 0) {
    snprintf(err_buf, err_buf_size,
             "msgpack_pack_str_body 'is_master' KEY failed.");
    return false;
  }
  // value
  int pack_bool_res = -1;
  if (user->is_master) {
    pack_bool_res = msgpack_pack_true(&pk);
  } else {
    pack_bool_res = msgpack_pack_false(&pk);
  }

  if (pack_bool_res != 0) {
    snprintf(err_buf, err_buf_size,
             "msgpack_pack true/false 'is_master' VALUE failed.");
    return false;
  }

  return true;
}

bool ifb_fun_try_unpack_user_info(const uint8_t *data, size_t size,
                                  ifb_strct_user_info *out_user_info,
                                  char *err_buf, size_t err_buf_size) {
  msgpack_unpacked result;
  msgpack_unpacked_init(&result);

  size_t offset = 0;
  switch (msgpack_unpack_next(&result, (const char *)data, size, &offset)) {
  case MSGPACK_UNPACK_NOMEM_ERROR: {
    snprintf(err_buf, err_buf_size, "msgpack_unpack_next() err: NOMEM_ERROR");
    return false;
  }
  case MSGPACK_UNPACK_PARSE_ERROR: {
    snprintf(err_buf, err_buf_size, "msgpack_unpack_next() err: PARSE_ERROR");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  case MSGPACK_UNPACK_CONTINUE: {
    snprintf(err_buf, err_buf_size,
             "msgpack_unpack_next() err: UNPACK_CONTINUE");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  default: {
    break;
  }
  }

  msgpack_object root = result.data;

  if (root.type != MSGPACK_OBJECT_MAP) {
    msgpack_unpacked_destroy(&result);
    return false;
  }

  ifb_strct_user_info unpack_res = {0};

  bool is_f_id_parsed = false;
  bool is_f_is_master_parsed = false;
  const char *id_key = "id";
  const char *is_master_key = "is_master";
  const size_t id_key_len = strlen(id_key);
  const size_t is_master_key_len = strlen(is_master_key);

  for (uint32_t i = 0; i < root.via.map.size; i++) {
    msgpack_object k = root.via.map.ptr[i].key;
    msgpack_object v = root.via.map.ptr[i].val;

    if (k.type != MSGPACK_OBJECT_STR) {
      continue;
    }

    const char *key_str = k.via.str.ptr;
    size_t key_str_len = k.via.str.size;
    if (key_str_len == id_key_len && memcmp(key_str, id_key, id_key_len) == 0) {
      if (v.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
        unpack_res.id = (uint32_t)v.via.u64;
        is_f_id_parsed = true;
      }
    } else if (key_str_len == is_master_key_len &&
               memcmp(key_str, is_master_key, is_master_key_len) == 0) {
      if (v.type == MSGPACK_OBJECT_BOOLEAN) {
        unpack_res.is_master = v.via.boolean;
        is_f_is_master_parsed = true;
      }
    }
  }

  if (!is_f_id_parsed || !is_f_is_master_parsed) {
    snprintf(err_buf, err_buf_size,
             "msgpack_unpack_next() err: some value not found.");
    msgpack_unpacked_destroy(&result);
    return false;
  }

  *out_user_info = unpack_res;
  msgpack_unpacked_destroy(&result);
  return true;
}

bool ifb_try_pack_str_full(msgpack_packer *pk, const char *str_value,
                           char *err_buf, size_t err_buf_size) {
  size_t str_len = strlen(str_value);
  if (msgpack_pack_str(pk, str_len) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_str failed for %s.\n",
             str_value);
    return false;
  }
  if (msgpack_pack_str_body(pk, str_value, str_len) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_str_body failed for %s.\n",
             str_value);
    return false;
  }

  return true;
}

bool ifb_try_pack_player_pos(msgpack_packer *pk,
                             const ifb_strct_player_pos *pos, char *err_buf,
                             size_t err_buf_size) {
  size_t keys_count = 3;
  if (msgpack_pack_map(pk, keys_count) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_map() failed.");
    return false;
  }

  // key
  const char *is_exist_key = "is_exists";
  if (!ifb_try_pack_str_full(pk, is_exist_key, err_buf, err_buf_size)) {
    return false;
  }

  // value
  int pack_res = -1;
  if (pos->is_exists) {
    pack_res = msgpack_pack_true(pk);
  } else {
    pack_res = msgpack_pack_false(pk);
  }

  if (pack_res != 0) {
    snprintf(err_buf, err_buf_size,
             "msgpack_pack_true/false 'is_exists' VALUE failed.");
    return false;
  }

  // key
  const char *x_key = "x";
  if (!ifb_try_pack_str_full(pk, x_key, err_buf, err_buf_size)) {
    return false;
  }
  // value
  if (msgpack_pack_int32(pk, pos->x) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_int32 'x' VALUE failed.");
    return false;
  }

  // key
  const char *y_key = "y";
  if (!ifb_try_pack_str_full(pk, y_key, err_buf, err_buf_size)) {
    return false;
  }
  // value
  if (msgpack_pack_int32(pk, pos->y) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_int32 'y' VALUE failed.");
    return false;
  }

  return true;
}

bool ifb_try_pack_player_pos_count(msgpack_packer *pk,
                                   ifb_strct_player_pos **pos_ptr_arr,
                                   size_t pos_ptr_arr_size, char *err_buf,
                                   size_t err_buf_size) {

  char buf_key[32];
  for (size_t i = 0; i < pos_ptr_arr_size; i++) {

    ifb_strct_player_pos *p = pos_ptr_arr[i];

    if (!p) {
      snprintf(err_buf, err_buf_size, "invalid ptr at index %lu", i);
      return false;
    }

    snprintf(buf_key, sizeof(buf_key), "pos_%ld", i + 1);
    // printf("pack player with key: %s\n", buf_key);
    if (!ifb_try_pack_str_full(pk, buf_key, err_buf, err_buf_size)) {
      return false;
    }
    if (!ifb_try_pack_player_pos(pk, p, err_buf, err_buf_size)) {
      return false;
    }
  }

  return true;
}

bool ifb_try_pack_game_state(msgpack_sbuffer *sbuf,
                             ifb_strct_game_state *game_state, char *err_buf,
                             size_t err_buf_size) {

  msgpack_packer pk = {0};
  msgpack_packer_init(&pk, sbuf, msgpack_sbuffer_write);

  size_t keys_count = 5;
  if (msgpack_pack_map(&pk, keys_count) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_map() failed.");
    return false;
  }

  // key
  const char *tick_key = "tick";
  if (!ifb_try_pack_str_full(&pk, tick_key, err_buf, err_buf_size)) {
    return false;
  }
  // value
  if (msgpack_pack_uint32(&pk, game_state->tick) != 0) {
    snprintf(err_buf, err_buf_size, "msgpack_pack_uint32 'tick' VALUE failed.");
    return false;
  }

  size_t pos_count = 4;
  ifb_strct_player_pos *positions[pos_count];
  positions[0] = &game_state->pos_1;
  positions[1] = &game_state->pos_2;
  positions[2] = &game_state->pos_3;
  positions[3] = &game_state->pos_4;

  if (!ifb_try_pack_player_pos_count(&pk, positions, pos_count, err_buf,
                                     err_buf_size)) {
    return false;
  }

  return true;
}

bool ifb_try_unpack_player_pos(msgpack_object obj, ifb_strct_player_pos *out,
                               char *err_buf, size_t err_buf_size) {

  if (obj.type != MSGPACK_OBJECT_MAP) {
    snprintf(err_buf, err_buf_size, "invalid obj type: %u", (uint8_t)obj.type);
    return false;
  }

  const char *is_exists_key = "is_exists";
  const size_t is_exists_key_len = strlen(is_exists_key);

  const char *x_key = "x";
  const char *y_key = "y";
  const size_t pos_key_length = strlen(x_key);

  bool is_f_is_exists_parsed = false;
  bool is_f_x_parsed = false;
  bool is_f_y_parsed = false;

  for (uint32_t i = 0; i < obj.via.map.size; i++) {
    msgpack_object_kv *kv = &obj.via.map.ptr[i];

    const char *key_str = kv->key.via.str.ptr;
    if (memcmp(key_str, is_exists_key, is_exists_key_len) == 0) {
      if (kv->val.type == MSGPACK_OBJECT_BOOLEAN) {
        out->is_exists = kv->val.via.boolean;
        is_f_is_exists_parsed = true;
      }
    }

    if (memcmp(key_str, x_key, pos_key_length) == 0) {
      out->x = (int32_t)kv->val.via.i64;
      is_f_x_parsed = true;
    }

    if (memcmp(key_str, y_key, pos_key_length) == 0) {
      out->y = (int32_t)kv->val.via.i64;
      is_f_y_parsed = true;
    }
  }

  if (!is_f_is_exists_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.",
             is_exists_key);
    return false;
  }

  if (!is_f_x_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", x_key);
    return false;
  }

  if (!is_f_y_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", y_key);
    return false;
  }

  return true;
}

bool ifb_try_unpack_game_state(const uint8_t *data, size_t data_size,
                               ifb_strct_game_state *out_game_state,
                               char *err_buf, size_t err_buf_size,
                               bool is_log_enabled) {

  if (!data) {
    snprintf(err_buf, err_buf_size, "data is NULL!");
    return false;
  }
  msgpack_unpacked result = {0};
  msgpack_unpacked_init(&result);

  size_t offset = 0;
  switch (
      msgpack_unpack_next(&result, (const char *)data, data_size, &offset)) {
  case MSGPACK_UNPACK_NOMEM_ERROR: {
    snprintf(err_buf, err_buf_size, "msgpack_unpack_next() err: NOMEM_ERROR");
    return false;
  }
  case MSGPACK_UNPACK_PARSE_ERROR: {
    snprintf(err_buf, err_buf_size, "msgpack_unpack_next() err: PARSE_ERROR");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  case MSGPACK_UNPACK_CONTINUE: {
    snprintf(err_buf, err_buf_size, "msgpack_unpack_next() err: CONTINUE");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  default: {
    break;
  }
  }

  msgpack_object root = result.data;
  if (root.type != MSGPACK_OBJECT_MAP) {
    snprintf(err_buf, err_buf_size, "invalid root type: %u",
             (uint8_t)root.type);
    msgpack_unpacked_destroy(&result);
    return false;
  }

  if (is_log_enabled) {
    puts("\n** ifb_try_unpack_game_state: unpacked obj **\n");
    msgpack_object_print(stdout, root);
    puts("\n** ************ ************ ************ ** **");
  }
  ifb_strct_game_state unpack_res = {0};

  bool is_f_tick_parsed = false;
  bool is_f_pos_1_parsed = false;
  bool is_f_pos_2_parsed = false;
  bool is_f_pos_3_parsed = false;
  bool is_f_pos_4_parsed = false;

  const char *tick_key = "tick";
  const char *pos_1_key = "pos_1";
  const char *pos_2_key = "pos_2";
  const char *pos_3_key = "pos_3";
  const char *pos_4_key = "pos_4";

  size_t tick_key_len = strlen(tick_key);
  size_t pos_key_len = strlen(pos_1_key);

  printf("map size: %u\n", root.via.map.size);

  for (uint32_t i = 0; i < root.via.map.size; i++) {
    msgpack_object k = root.via.map.ptr[i].key;
    msgpack_object v = root.via.map.ptr[i].val;

    if (k.type != MSGPACK_OBJECT_STR) {
      continue;
    }

    const char *key_str = k.via.str.ptr;
    size_t key_str_len = k.via.str.size;
    if (key_str_len == tick_key_len &&
        memcmp(key_str, tick_key, tick_key_len) == 0) {
      if (v.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
        unpack_res.tick = (uint32_t)v.via.u64;
        is_f_tick_parsed = true;
      }
    } else if (key_str_len == pos_key_len) {

      if (memcmp(key_str, pos_1_key, pos_key_len) == 0) {
        if (!ifb_try_unpack_player_pos(v, &unpack_res.pos_1, err_buf,
                                       err_buf_size)) {
          msgpack_unpacked_destroy(&result);
          return false;
        }
        is_f_pos_1_parsed = true;
      }
      if (memcmp(key_str, pos_2_key, pos_key_len) == 0) {
        if (!ifb_try_unpack_player_pos(v, &unpack_res.pos_2, err_buf,
                                       err_buf_size)) {
          msgpack_unpacked_destroy(&result);
          return false;
        }
        is_f_pos_2_parsed = true;
      }
      if (memcmp(key_str, pos_3_key, pos_key_len) == 0) {
        if (!ifb_try_unpack_player_pos(v, &unpack_res.pos_3, err_buf,
                                       err_buf_size)) {
          msgpack_unpacked_destroy(&result);
          return false;
        }
        is_f_pos_3_parsed = true;
      }
      if (memcmp(key_str, pos_4_key, pos_key_len) == 0) {
        if (!ifb_try_unpack_player_pos(v, &unpack_res.pos_4, err_buf,
                                       err_buf_size)) {
          msgpack_unpacked_destroy(&result);
          return false;
        }
        is_f_pos_4_parsed = true;
      }
    }
  }

  if (!is_f_tick_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", tick_key);
    msgpack_unpacked_destroy(&result);
    return false;
  }

  if (!is_f_pos_1_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", pos_1_key);
    msgpack_unpacked_destroy(&result);
    return false;
  }
  if (!is_f_pos_2_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", pos_2_key);
    msgpack_unpacked_destroy(&result);
    return false;
  }
  if (!is_f_pos_3_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", pos_3_key);
    msgpack_unpacked_destroy(&result);
    return false;
  }
  if (!is_f_pos_4_parsed) {
    snprintf(err_buf, err_buf_size, "field: '%s' is not parsed.", pos_4_key);
    msgpack_unpacked_destroy(&result);
    return false;
  }

  *out_game_state = unpack_res;
  msgpack_unpacked_destroy(&result);
  return true;
}

bool ifb_try_pack_player_input(msgpack_sbuffer *sbuf,
                               ifb_strct_player_input player_input,
                               char *err_buf, size_t err_buf_size) {
  msgpack_packer pk;
  msgpack_packer_init(&pk, sbuf, msgpack_sbuffer_write);

  size_t keys_count = 3;
  if (msgpack_pack_map(&pk, keys_count) != 0) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_pack_player_input()->msgpack_pack_map() failed.");
    return false;
  }

  const char *id_key = "id";
  if (!ifb_try_pack_str_full(&pk, id_key, err_buf, err_buf_size)) {
    return false;
  }
  if (msgpack_pack_uint32(&pk, player_input.id) != 0) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_pack_player_input() pack player_input.id failed.");
    return false;
  }

  const char *x_key = "x";
  if (!ifb_try_pack_str_full(&pk, x_key, err_buf, err_buf_size)) {
    return false;
  }

  if (msgpack_pack_int32(&pk, player_input.x) != 0) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_pack_player_input() pack player_input.x failed.");
    return false;
  }

  const char *y_key = "y";
  if (!ifb_try_pack_str_full(&pk, y_key, err_buf, err_buf_size)) {
    return false;
  }

  if (msgpack_pack_int32(&pk, player_input.y) != 0) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_pack_player_input() pack player_input.y failed.");
    return false;
  }

  return true;
}

bool ifb_try_unpack_player_input(const uint8_t *data, size_t data_size,
                                 ifb_strct_player_input *out, char *err_buf,
                                 size_t err_buf_size) {

  msgpack_unpacked result = {0};
  msgpack_unpacked_init(&result);

  size_t offset = 0;

  switch (
      msgpack_unpack_next(&result, (const char *)data, data_size, &offset)) {
  case MSGPACK_UNPACK_NOMEM_ERROR: {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() unpack error: NOMEM_ERROR");
    return false;
  }
  case MSGPACK_UNPACK_PARSE_ERROR: {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() unpack error: PARSE_ERROR");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  case MSGPACK_UNPACK_CONTINUE: {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() unpack error: UNPACK_CONTINUE");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  default: {
    break;
  }
  }

  msgpack_object root = result.data;
  if (root.type != MSGPACK_OBJECT_MAP) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() invalid root type.");
    msgpack_unpacked_destroy(&result);
    return false;
  }

  ifb_strct_player_input unpack_res = {0};
  bool is_f_id_parsed = false;
  bool is_f_x_parsed = false;
  bool is_f_y_parsed = false;

  const char *id_key = "id";
  const char *x_key = "x";
  const char *y_key = "y";

  for (uint32_t i = 0; i < root.via.map.size; i++) {
    msgpack_object k = root.via.map.ptr[i].key;
    msgpack_object v = root.via.map.ptr[i].val;

    if (k.type != MSGPACK_OBJECT_STR) {
      continue;
    }

    const char *key_str = k.via.str.ptr;
    const uint32_t key_str_len = k.via.str.size;

    if (key_str_len == strlen(id_key) &&
        memcmp(key_str, id_key, key_str_len) == 0) {
      if (v.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
        unpack_res.id = v.via.u64;
        is_f_id_parsed = true;
      }
    }

    if (key_str_len == strlen(x_key)) {
      if (memcmp(key_str, x_key, key_str_len) == 0) {
        unpack_res.x = (int32_t)v.via.i64;
        is_f_x_parsed = true;
      }
      if (memcmp(key_str, y_key, key_str_len) == 0) {
        unpack_res.y = (int32_t)v.via.i64;
        is_f_y_parsed = true;
      }
    }
  }

  if (!is_f_id_parsed) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() 'id' is not parsed.");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  if (!is_f_x_parsed) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() 'x' is not parsed.");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  if (!is_f_y_parsed) {
    snprintf(err_buf, err_buf_size,
             "ifb_try_unpack_player_input() 'y' is not parsed.");
    msgpack_unpacked_destroy(&result);
    return false;
  }
  *out = unpack_res;
  msgpack_unpacked_destroy(&result);
  return true;
}