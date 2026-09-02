#include <enet/enet.h>
#include <msgpack.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "protocol_msg.h"

typedef struct {
  size_t index;
  ENetPeer *peer;
  bool is_exists;
  bool is_connected;
  bool is_master;
} strct_active_player;

const size_t ACTIVE_PLAYERS_SIZE = 10;
static strct_active_player **ACTIVE_PLAYERS;
static ENetHost *SERVER_HOST;

static bool is_player_exists_and_connected(strct_active_player *p) {
  return p != NULL && p->is_exists && p->is_connected;
}

static bool is_player_active(ENetPeer *peer) {

  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (!p->is_exists) {
      continue;
    }
    if (p->peer == peer && p->is_connected) {
      return true;
    }
  }

  return false;
}

static bool try_get_player_by_peer(ENetPeer *peer,
                                   strct_active_player **out_player) {
  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (!is_player_exists_and_connected(p)) {
      continue;
    }
    if (p->peer == peer) {
      *out_player = p;
      return true;
    }
  }

  return false;
}

static bool try_get_master_client(strct_active_player **out_player,
                                  char *err_buf, size_t err_buf_size) {

  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (!is_player_exists_and_connected(p)) {
      continue;
    }
    if (p->is_master) {
      *out_player = p;
      return true;
    }
  }
  snprintf(err_buf, err_buf_size, "master client is not found.");
  return false;
}

static bool try_register_player(strct_active_player *player, size_t *out_index,
                                char *error_buffer, size_t error_buf_size) {
  bool is_free_index_found = false;
  size_t free_index = 0;

  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (is_player_exists_and_connected(p)) {
      continue;
    }
    is_free_index_found = true;
    free_index = i;
    break;
  }

  if (!is_free_index_found) {
    snprintf(error_buffer, error_buf_size,
             "there is no free space for player.");
    return false;
  }

  strct_active_player *copy = malloc(sizeof(strct_active_player));
  *copy = *player;
  ACTIVE_PLAYERS[free_index] = copy;
  *out_index = free_index;
  return true;
}

static bool try_remove_player(ENetPeer *peer, char *error_buffer,
                              size_t error_buf_size) {
  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (!p->is_exists) {
      continue;
    }
    if (p->peer != peer) {
      continue;
    }
    p->peer = NULL;
    p->is_exists = false;
    return true;
  }
  snprintf(error_buffer, error_buf_size,
           "player with this ENetPeer pointer '%p' is not found!", peer);
  return false;
}

static size_t get_active_players_count() {
  size_t count = 0;
  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (!p->is_exists) {
      continue;
    }
    if (!p->is_connected) {
      continue;
    }
    count++;
  }

  return count;
}

static void print_err(const char *msg) { fprintf(stderr, "FATAL: %s\n", msg); }

// отправляет игровое состояние всем, кроме мастера.
// [OBSOLETE] , TODO: REMOVE
static void
old_handle_game_state_updated_by_master(ifb_strct_game_state game_state) {

  char err_buf[ERR_BUFF_SIZE];
  msgpack_sbuffer sbuf = {0};
  msgpack_sbuffer_init(&sbuf);

  if (!ifb_try_pack_game_state(&sbuf, &game_state, err_buf, ERR_BUFF_SIZE)) {
    fprintf(stderr, "ifb_try_pack_game_state() failed: %s\n", err_buf);
    return;
  }
  // TODO: enet_host_broadcast

  // enet_host_broadcast(SERVER_HOST, IFB_NET_CHANNEL_STREAM,)

  // return;
  // notify all slaves but  master

  // BOTTLENECK HERE
  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player *p = ACTIVE_PLAYERS[i];
    if (!is_player_exists_and_connected(p)) {
      continue;
    }
    if (p->is_master) {
      continue;
    }

    if (!ifb_ptcl_try_send_msg_packet(
            IFB_MSG_TYPE_GAME_STATE_UPD, IFB_NET_CHANNEL_STREAM, &sbuf, p->peer,
            SERVER_HOST, true, true, err_buf, ERR_BUFF_SIZE)) {
      fprintf(stderr, "ifb_ptcl_try_send_msg_packet() failed: %s\n", err_buf);
    }
  }
}

// отправляет игровое состояние всем.
static void
handle_game_state_updated_by_master(ifb_strct_game_state game_state) {

  char err_buf[ERR_BUFF_SIZE];
  msgpack_sbuffer sbuf = {0};
  msgpack_sbuffer_init(&sbuf);

  if (!ifb_try_pack_game_state(&sbuf, &game_state, err_buf, ERR_BUFF_SIZE)) {
    fprintf(stderr, "ifb_try_pack_game_state() failed: %s\n", err_buf);
    return;
  }
  bool is_reliable = false;
  bool is_forced = false;

  if (!ifb_ptcl_try_broadcast_msg_packet(
          IFB_MSG_TYPE_GAME_STATE_UPD, IFB_NET_CHANNEL_STREAM, &sbuf,
          SERVER_HOST, is_reliable, is_forced, err_buf, ERR_BUFF_SIZE)) {
    fprintf(stderr, "ifb_ptcl_try_broadcast_msg_packet() failed: %s\n",
            err_buf);
  }
}

static void handle_player_register(ENetPeer *peer) {

  if (is_player_active(peer)) {
    return;
  }

  strct_active_player p = {0};
  p.peer = peer;
  p.is_connected = true;
  p.is_exists = true;
  p.is_master = get_active_players_count() == 0 ? true : false;
  char err[ERR_BUFF_SIZE];
  size_t out_index = 555;
  if (!try_register_player(&p, &out_index, err, ERR_BUFF_SIZE)) {
    fprintf(stderr, "try_register_player() failed: %s\n", err);
    return;
  }

  msgpack_sbuffer sbuf = {0};
  msgpack_sbuffer_init(&sbuf);

  ifb_strct_user_info usr_info = {0};
  usr_info.id = out_index;
  usr_info.is_master = p.is_master;

  if (!ifb_try_pack_user_info(&sbuf, &usr_info, err, ERR_BUFF_SIZE)) {
    fprintf(stderr, "try_register_player() failed: %s\n", err);
    msgpack_sbuffer_destroy(&sbuf);
    return;
  }

  if (!ifb_ptcl_try_send_msg_packet(
          IFB_MSG_TYPE_RES_USER_INFO, IFB_NET_CHANNEL_CRITICAL, &sbuf, peer,
          SERVER_HOST, true, true, err, ERR_BUFF_SIZE)) {
    fprintf(stderr, "try_register_player() failed: %s\n", err);
    msgpack_sbuffer_destroy(&sbuf);
    return;
  }

  // отправлем мастеру событие подключения игрока
  if (p.is_master == false) {
    strct_active_player *master = NULL;
    if (!try_get_master_client(&master, err, ERR_BUFF_SIZE)) {
      fprintf(stderr, "try_get_master_client() failed! %s\n", err);
      msgpack_sbuffer_destroy(&sbuf);
      return;
    }
    if (!ifb_ptcl_try_send_msg_packet(
            IFB_MSG_TYPE_SLAVE_CONNECTED, IFB_NET_CHANNEL_CRITICAL, &sbuf,
            master->peer, SERVER_HOST, true, true, err, ERR_BUFF_SIZE)) {
      fprintf(stderr, "ifb_ptcl_try_send_msg_packet() failed: %s\n", err);
      msgpack_sbuffer_destroy(&sbuf);
      return;
    }
  }

  msgpack_sbuffer_destroy(&sbuf);
  printf("player registered, peer: %p, index: %lu, is_master?: %s\n", peer,
         out_index, p.is_master ? "TRUE" : "FALSE");
}

int main(void) {
  if (enet_initialize() != 0) {
    print_err("enet_initialize failed.");
    exit(EXIT_FAILURE);
  }
  atexit(enet_deinitialize);

  ACTIVE_PLAYERS = malloc(ACTIVE_PLAYERS_SIZE * sizeof(strct_active_player));

  for (size_t i = 0; i < ACTIVE_PLAYERS_SIZE; i++) {
    strct_active_player p = {0};
    p.index = i;
    ACTIVE_PLAYERS[i] = &p;
  }

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = PTCL_PORT;

  ENetHost *server = enet_host_create(&address,
                                      32, // max clients
                                      IFB_CNST_NET_CHANNELS_COUNT, // channels
                                      0, // incoming bandwidth (0 = unlimited)
                                      0  // outgoing bandwidth
  );

  if (!server) {
    print_err("enet_host_create() failed.");
    exit(EXIT_FAILURE);
  }

  SERVER_HOST = server;
  printf("Server listening on 0.0.0.0:7777\n");

  while (1) {
    ENetEvent event;
    while (enet_host_service(server, &event, 16) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT: {
        // 0xFF = 255
        printf("Client connected: %u.%u.%u.%u:%u\n",
               (event.peer->address.host) & 0xFF,
               (event.peer->address.host >> 8) & 0xFF,
               (event.peer->address.host >> 16) & 0xFF,
               (event.peer->address.host >> 24) & 0xFF,
               event.peer->address.port);
        printf("\t connectID: %u\n", event.peer->connectID);
        break;
      }

      case ENET_EVENT_TYPE_RECEIVE: {

        ifb_en_message_type msg_type = IFB_MSG_TYPE_NONE;
        uint8_t *payload = NULL;
        size_t payload_size = 0;
        char err_buf[ERR_BUFF_SIZE];
        if (ifb_ptcl_try_parse_message_by_type(&event, &msg_type, &payload,
                                               &payload_size, err_buf,
                                               ERR_BUFF_SIZE)) {
          printf("\t\tparsed client msg-> type:%s, data_null?:%s\n",
                 ifb_msg_type_to_str(msg_type),
                 payload == NULL ? "true" : "false");
          strct_active_player *player = NULL;
          if (try_get_player_by_peer(event.peer, &player)) {
            printf("\t\t\tfrom player: (id: %ld, master?: %d)\n", player->index,
                   player->is_master);
          }

          // unpack msgpack functions here
          switch (msg_type) {
          case IFB_MSG_TYPE_REQ_USER_INFO: {
            handle_player_register(event.peer);
            break;
          }
          case IFB_MSG_TYPE_GAME_STATE_UPD: {
            ifb_strct_game_state game_state = {};
            if (!ifb_try_unpack_game_state(payload, payload_size, &game_state,
                                           err_buf, ERR_BUFF_SIZE, false)) {
              fprintf(stderr, "ifb_try_unpack_game_state err!: %s\n", err_buf);
              break;
            }
            handle_game_state_updated_by_master(game_state);
            break;
          }
          case IFB_MSG_TYPE_PLAYER_INPUT: {

            // TODO: Сервер просто не справляется, когда приходит инпут его
            // процесс затупляется здесь.
            //  То ли из-за пакетов, то-ли из-за try_get_master_client() - НЕТ, проверено 02.09.26 кэшированием указателя на мастера.
            //  Или забивается канал IFB_NET_CHANNEL_STREAM - НЕТ, заменил канал передачи на новый - IFB_NET_CHANNEL_STREAM_PUPPET_INPUTS
            //  XXX Test
            // break;
            //  переправляем мастеру

            // TODO: Есть архитектурная проблема. Сервер забивает свой поток обработкой пакетов от инпутов паппетов и поэтому обновления от мастера перебиваются - пиры отключаются и дохнут. Нельзя слать в такой сервер одновременно столько пакетов, он не справляется.
            char err_buf[ERR_BUFF_SIZE];
            strct_active_player *master = NULL;
            if (!try_get_master_client(&master, err_buf, ERR_BUFF_SIZE)) {
              fprintf(stderr, "try_get_master_client fail: %s\n", err_buf);
              break;
              ;
            }
            if (!ifb_try_relay_packet(SERVER_HOST, event.packet, master->peer,
                                      IFB_NET_CHANNEL_STREAM_PUPPET_INPUTS, false, false,
                                      err_buf, ERR_BUFF_SIZE)) {
              fprintf(stderr, "ifb_try_relay_packet fail: %s\n", err_buf);
              break;
            }
            break;
          }
          default: {

            break;
          }
          }
        } else {
          printf("Unable to parse incoming message: %s\n", err_buf);
        }
        enet_packet_destroy(event.packet);
        break;
      }

      case ENET_EVENT_TYPE_DISCONNECT: {
        printf("client disconnected.\n");
        char err[ERR_BUFF_SIZE];
        if (!try_remove_player(event.peer, err, ERR_BUFF_SIZE)) {
          printf("try_remove_player err %s\n", err);
        }

        break;
      }

      default:
        break;
      }
    }
  }

  return EXIT_SUCCESS;
}