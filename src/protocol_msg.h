#pragma once
#include <enet/enet.h>
#include <msgpack.h>

static const uint16_t PTCL_PORT = 7777;
static const size_t ERR_BUFF_SIZE = 128;
static const uint8_t IFB_CNST_NET_CHANNELS_COUNT = 3;

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
                                  size_t error_buf_size);

bool ifb_try_relay_packet(ENetPacket *pkt_src, ENetPeer *peer_to,
                          ifb_en_channel_id channel_id, bool is_reliable,
                          char *err_buf, size_t err_buf_size);

const char *ifb_msg_type_to_str(ifb_en_message_type msg_type);

bool ifb_ptcl_try_parse_message_by_type(ENetEvent *event,
                                        ifb_en_message_type *out_msg_type,
                                        uint8_t **out_only_msg_payload,
                                        size_t *out_only_msg_payload_size,
                                        char *err_buf, size_t err_buf_size);

bool ifb_try_pack_user_info(msgpack_sbuffer *sbuf, ifb_strct_user_info *user,
                            char *err_buf, size_t err_buf_size);

bool ifb_fun_try_unpack_user_info(const uint8_t *data, size_t size,
                                  ifb_strct_user_info *out_user_info,
                                  char *err_buf, size_t err_buf_size);

bool ifb_try_pack_str_full(msgpack_packer *pk, const char *str_value,
                           char *err_buf, size_t err_buf_size);

bool ifb_try_pack_player_pos(msgpack_packer *pk,
                             const ifb_strct_player_pos *pos, char *err_buf,
                             size_t err_buf_size);

bool ifb_try_pack_player_pos_count(msgpack_packer *pk,
                                   ifb_strct_player_pos **pos_ptr_arr,
                                   size_t pos_ptr_arr_size, char *err_buf,
                                   size_t err_buf_size);

bool ifb_try_pack_game_state(msgpack_sbuffer *sbuf,
                             ifb_strct_game_state *game_state, char *err_buf,
                             size_t err_buf_size);

bool ifb_try_unpack_player_pos(msgpack_object obj, ifb_strct_player_pos *out,
                               char *err_buf, size_t err_buf_size);

bool ifb_try_unpack_game_state(const uint8_t *data, size_t data_size,
                               ifb_strct_game_state *out_game_state,
                               char *err_buf, size_t err_buf_size,
                               bool is_log_enabled);

bool ifb_try_pack_player_input(msgpack_sbuffer *sbuf,
                               ifb_strct_player_input player_input,
                               char *err_buf, size_t err_buf_size);

bool ifb_try_unpack_player_input(const uint8_t *data, size_t data_size,
                                 ifb_strct_player_input *out, char *err_buf,
                                 size_t err_buf_size);