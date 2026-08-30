#include <enet/enet.h>
#include <msgpack.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#include "help_func/math_and_time.h"
#include "ifb_syscall_safe.h"
#include "protocol_msg.h"
#include "thread_safe/safe_queue.h"

typedef enum {
  CONNECT = 0,
  DISCONNECT_N_FREE = 1,
  REQ_USER_INFO = 2,
  TICK_SLAVE = 3,
  TICK_MC = 4,
} strct_client_command_type;

typedef struct strct_client_command_msg {
  strct_client_command_type type;
  void *data;
} strct_client_command_msg;

typedef struct strct_msg_connect {
  char *host;
  int port;
} strct_msg_connect;

typedef struct strct_user_input {
  int x;
  int y;
} strct_user_input;

// shared
static const size_t FRAME_RATE = 30;

static const char *NET_SUFFIX = "\t<<<NET_THREAD>>> ";

static const size_t OTHERS_COUNT = 3;

static ifb_strct_safe_queue *LOCAL_COMMANDS_SF_QUEUE;
static atomic_bool IS_APP_RUNNING;

static pthread_mutex_t MTX_CURRENT_GAME_STATE;
static ifb_strct_game_state *CURRENT_GAME_STATE;

static pthread_mutex_t MTX_MY_USER_INFO;
static ifb_strct_user_info *MY_USER_INFO;

static pthread_mutex_t MTX_LAST_PLAYER_INPUTS;
static ifb_strct_player_input *LAST_PLAYER_INPUTS;
// end

// net thread
static bool IS_DISCONNECTED = false;

static ENetHost *CLIENT_HOST;
static ENetPeer *CLIENT_PEER;
// end

static void print_err(const char *msg) { fprintf(stderr, "FATAL: %s\n", msg); }

static void enqueue_cmd(strct_client_command_type type, void *data) {
  strct_client_command_msg *msg = malloc(sizeof(strct_client_command_msg));
  msg->type = type;
  msg->data = data;

  ifb_safe_queue_push(LOCAL_COMMANDS_SF_QUEUE, (void *)msg);
}

static void
update_games_state_by_slave_input(ifb_strct_player_input player_input) {

  pthread_mutex_lock(&MTX_LAST_PLAYER_INPUTS);

  switch (player_input.id) {
  case 1: {
    LAST_PLAYER_INPUTS[0].x = player_input.x;
    LAST_PLAYER_INPUTS[0].y = player_input.y;
    break;
  }
  case 2: {
    LAST_PLAYER_INPUTS[1].x = player_input.x;
    LAST_PLAYER_INPUTS[1].y = player_input.y;
    break;
  }
  case 3: {
    LAST_PLAYER_INPUTS[2].x = player_input.x;
    LAST_PLAYER_INPUTS[2].y = player_input.y;
    break;
  }
  default: {
    fprintf(stderr, "invalid player id: %d, can't update state.\n",
            player_input.id);
    break;
  }
  }

  pthread_mutex_unlock(&MTX_LAST_PLAYER_INPUTS);
}

static void attempt_init_current_game_state_by_master() {

  pthread_mutex_lock(&MTX_CURRENT_GAME_STATE);
  if (!CURRENT_GAME_STATE) {
    ifb_strct_game_state game_state = {0};
    CURRENT_GAME_STATE = malloc(sizeof(ifb_strct_game_state));
    *CURRENT_GAME_STATE = game_state;
  }
  pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);

  pthread_mutex_lock(&MTX_LAST_PLAYER_INPUTS);
  if (!LAST_PLAYER_INPUTS) {
    LAST_PLAYER_INPUTS = malloc(sizeof(ifb_strct_player_input) * OTHERS_COUNT);
    if (LAST_PLAYER_INPUTS == NULL) {
      fprintf(stderr, "attempt_init_current_game_state_by_master(), FATAL: "
                      "malloc() failed!\n");
      pthread_mutex_unlock(&MTX_LAST_PLAYER_INPUTS);
      return;
    }
    for (size_t i = 0; i < OTHERS_COUNT; i++) {
      ifb_strct_player_input player_input = {0};
      player_input.id = i + 1;
      LAST_PLAYER_INPUTS[i] = player_input;
    }
  }
  pthread_mutex_unlock(&MTX_LAST_PLAYER_INPUTS);
}

static void handle_on_slave_connected(ifb_strct_user_info user_info) {

  uint32_t user_id = user_info.id;
  pthread_mutex_lock(&MTX_CURRENT_GAME_STATE);

  if (CURRENT_GAME_STATE == NULL) {
    fprintf(stderr,
            "handle_on_slave_connected(): CURRENT_GAME_STATE is NULL\n");
    pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);
    return;
  }

  if (user_id == 1) {
    CURRENT_GAME_STATE->pos_2.is_exists = true;
  } else if (user_id == 2) {
    CURRENT_GAME_STATE->pos_3.is_exists = true;
  } else if (user_id == 3) {
    CURRENT_GAME_STATE->pos_4.is_exists = true;
  } else {
    fprintf(stderr, "handle_on_slave_connected(): unknown player id: %d\n",
            user_id);
  }

  pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);
}

static void
handle_on_slave_game_state_updated(ifb_strct_game_state game_state) {
  puts("\n*** *** game_state updated *** ***\n");
  printf("\t tick: %d\n", game_state.tick);
  printf("\t pos_1: ( exists: %d, x: %d, y: %d )\n", game_state.pos_1.is_exists,
         game_state.pos_1.x, game_state.pos_1.y);
  printf("\t pos_2: ( exists: %d, x: %d, y: %d )\n", game_state.pos_2.is_exists,
         game_state.pos_2.x, game_state.pos_2.y);
  printf("\t pos_3: ( exists: %d, x: %d, y: %d )\n", game_state.pos_3.is_exists,
         game_state.pos_3.x, game_state.pos_3.y);
  printf("\t pos_4: ( exists: %d, x: %d, y: %d )\n", game_state.pos_4.is_exists,
         game_state.pos_4.x, game_state.pos_4.y);
  puts("\n*** *** *** *** *** *** ** *** ***\n");
}

static void handle_cmd_slave_tick(strct_user_input *user_input) {

  msgpack_sbuffer sbuf = {0};
  msgpack_sbuffer_init(&sbuf);

  ifb_strct_player_input input = {0};

  pthread_mutex_lock(&MTX_MY_USER_INFO);
  input.id = MY_USER_INFO->id;
  pthread_mutex_unlock(&MTX_MY_USER_INFO);

  input.x = user_input->x;
  input.y = user_input->y;

  char err_buf[ERR_BUFF_SIZE];

  if (!ifb_try_pack_player_input(&sbuf, input, err_buf, ERR_BUFF_SIZE)) {
    fprintf(stderr, "handle_cmd_slave_tick() err:\n\t%s\n", err_buf);
    msgpack_sbuffer_destroy(&sbuf);
    return;
  }

  if (!ifb_ptcl_try_send_msg_packet(
          IFB_MSG_TYPE_PLAYER_INPUT, IFB_NET_CHANNEL_STREAM, &sbuf, CLIENT_PEER,
          CLIENT_HOST, true, true, err_buf, ERR_BUFF_SIZE)) {
    fprintf(stderr, "handle_cmd_slave_tick() err:\n\t%s\n", err_buf);
    msgpack_sbuffer_destroy(&sbuf);
    return;
  }

  msgpack_sbuffer_destroy(&sbuf);
}

static void handle_cmd_disconnect_n_free() {
  if (IS_DISCONNECTED) {
    return;
  }

  enet_peer_disconnect(CLIENT_PEER, 0);
  ENetEvent event;
  while (enet_host_service(CLIENT_HOST, &event, 1000) > 0) {
    if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      printf("Disconnected.\n");
      break;
      ;
    }
  }
  enet_host_destroy(CLIENT_HOST);
  CLIENT_PEER = NULL;
  CLIENT_HOST = NULL;
  IS_DISCONNECTED = true;
}

static void handle_cmd_connect_to_host(strct_msg_connect *msg) {

  ENetHost *client = enet_host_create(NULL, // client identified with NULL
                                      1,    // only outgoing connection
                                      IFB_CNST_NET_CHANNELS_COUNT, // channels
                                      0, 0);

  if (!client) {
    print_err("enet_host_create(client) failed.");
    return;
  }

  printf("trying to connect host: %s:%d\n ...", msg->host, msg->port);

  ENetAddress address;
  if (enet_address_set_host(&address, msg->host) != 0) {
    print_err("enet_address_set_host() failed.");
    return;
  }

  address.port = msg->port;

  ENetPeer *peer =
      enet_host_connect(client, &address, IFB_CNST_NET_CHANNELS_COUNT, 0);

  if (!peer) {
    print_err("enet_host_connect() failed.");
    return;
  }

  // max, min, max timeout in sec to detect server disconnect
  enet_peer_timeout(peer, 3000, 1000, 3000);

  // wait for connect
  ENetEvent event;
  int wait_timeout_ms = 3000;
  bool is_connected = false;

  while (enet_host_service(client, &event, wait_timeout_ms) > 0) {
    switch (event.type) {
    case (ENET_EVENT_TYPE_CONNECT): {
      printf("connected to %s:%d\n", msg->host, msg->port);
      is_connected = true;
      break;
    }
    default: {
      break;
    }
    }
  };

  if (!is_connected) {
    enet_peer_reset(peer);
    print_err("Connect timeout");
    return;
  }

  CLIENT_HOST = client;
  CLIENT_PEER = peer;
}

static void handle_net_events_loop() {
  ENetEvent event;
  int r = enet_host_service(CLIENT_HOST, &event, 100);
  if (r < 0) {
    print_err("socket error.");
    return;
  }

  if (r == 0) {
    // timeout
    return;
  }
  switch (event.type) {
  case ENET_EVENT_TYPE_RECEIVE: {
    ifb_en_message_type msg_type = IFB_MSG_TYPE_NONE;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    char err_buf[ERR_BUFF_SIZE];
    if (ifb_ptcl_try_parse_message_by_type(&event, &msg_type, &payload,
                                           &payload_size, err_buf,
                                           ERR_BUFF_SIZE)) {
      printf("received msg-> type:%s, data_null?:%s\n",
             ifb_msg_type_to_str(msg_type), payload == NULL ? "true" : "false");
      // unpack msgpack functions here
      switch (msg_type) {
      case IFB_MSG_TYPE_RES_USER_INFO: {

        ifb_strct_user_info usr_info = {0};
        if (ifb_fun_try_unpack_user_info(payload, payload_size, &usr_info,
                                         err_buf, ERR_BUFF_SIZE)) {
          pthread_mutex_lock(&MTX_MY_USER_INFO);
          if (MY_USER_INFO != NULL) {
            free(MY_USER_INFO);
          }
          MY_USER_INFO = malloc(sizeof(ifb_strct_user_info));
          *MY_USER_INFO = usr_info;

          printf("MY_USER_INFO parsed: (id: %d, is_master: %s)\n",
                 MY_USER_INFO->id, MY_USER_INFO->is_master ? "TRUE" : "FALSE");
          pthread_mutex_unlock(&MTX_MY_USER_INFO);

          if (usr_info.is_master) {
            attempt_init_current_game_state_by_master();
          }

        } else {
          printf("can't parse ifb_strct_user_info: %s\n", err_buf);
        }

        break;
      }
      case IFB_MSG_TYPE_GAME_STATE_UPD: {

        ifb_strct_game_state game_state = {0};
        if (ifb_try_unpack_game_state(payload, payload_size, &game_state,
                                      err_buf, ERR_BUFF_SIZE, false)) {
          handle_on_slave_game_state_updated(game_state);
        } else {
          fprintf(stderr, "ifb_try_unpack_game_state failed: %s\n", err_buf);
        }
        break;
      }
      case (IFB_MSG_TYPE_SLAVE_CONNECTED): {

        ifb_strct_user_info ui = {0};
        if (ifb_fun_try_unpack_user_info(payload, payload_size, &ui, err_buf,
                                         ERR_BUFF_SIZE)) {
          printf("slave connected: (id: %d, is_master: %d)\n", ui.id,
                 ui.is_master);
          handle_on_slave_connected(ui);
        } else {
          fprintf(stderr, "ifb_fun_try_unpack_user_info failed: %s\n", err_buf);
        }
        break;
      }
      case (IFB_MSG_TYPE_PLAYER_INPUT): {
        // от слейвов мастеру приходят
        ifb_strct_player_input player_input = {0};
        if (ifb_try_unpack_player_input(payload, payload_size, &player_input,
                                        err_buf, ERR_BUFF_SIZE)) {
          printf("received player input: (id: %d, x: %d, y: %d)\n",
                 player_input.id, player_input.x, player_input.y);
          update_games_state_by_slave_input(player_input);

        } else {
          fprintf(stderr, "ifb_try_unpack_player_input failed: %s\n", err_buf);
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
  default: {
    break;
  }
  }
}

static void *net_bg_thread(void *arg) {
  (void)arg; // supress warning

  struct timespec ts = ifb_mat_timespec_ms(500);

  while (atomic_load(&IS_APP_RUNNING)) {

    if (IS_DISCONNECTED) {
      thrd_sleep(&ts, NULL);
      continue;
    }
    // incoming net events loop
    if (CLIENT_HOST != NULL) {
      handle_net_events_loop();
    }

    // messages from main thread loop
    void *queue_data = NULL;
    if (ifb_safe_queue_try_pop(LOCAL_COMMANDS_SF_QUEUE, &queue_data)) {

      strct_client_command_msg *cmd_msg =
          (strct_client_command_msg *)queue_data;
      if (cmd_msg == NULL) {
        print_err("cmd_msg is NULL");
        continue;
      }

      switch (cmd_msg->type) {
      case CONNECT: {
        strct_msg_connect *msg = (strct_msg_connect *)cmd_msg->data;
        if (msg == NULL) {
          print_err("cannot resolve strct_msg_connect* pointer.");
          continue;
        }
        printf("\tCMD CONNECT: argz ( %s, %d )\n", msg->host, msg->port);

        handle_cmd_connect_to_host(msg);

        free(msg->host);
        free(msg);

        break;
      }
      case DISCONNECT_N_FREE: {

        printf("\tCMD DISCONNECT_N_FREE\n");

        handle_cmd_disconnect_n_free();
        break;
      }
      case REQ_USER_INFO: {
        printf("\tCMD REQ_USER_INFO\n");

        char err[ERR_BUFF_SIZE];

        if (!ifb_ptcl_try_send_msg_packet(
                IFB_MSG_TYPE_REQ_USER_INFO, IFB_NET_CHANNEL_CRITICAL, NULL,
                CLIENT_PEER, CLIENT_HOST, true, true, err, ERR_BUFF_SIZE)) {
          printf("%s send_msg_packet_to_server error: %s\n", NET_SUFFIX, err);
        }

        break;
      }
      case TICK_MC: {
        printf("\tCMD TICK_MC\n");

        char err[ERR_BUFF_SIZE];
        msgpack_sbuffer sbuf = {0};
        msgpack_sbuffer_init(&sbuf);

        pthread_mutex_lock(&MTX_CURRENT_GAME_STATE);
        if (!CURRENT_GAME_STATE) {
          printf("CURRENT_GAME_STATE is NULL\n!");
          pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);
          break;
        }
        ifb_strct_game_state game_state = *CURRENT_GAME_STATE;
        pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);

        if (!ifb_try_pack_game_state(&sbuf, &game_state, err, ERR_BUFF_SIZE)) {
          printf("%s ifb_try_pack_game_state error: %s\n", NET_SUFFIX, err);
          msgpack_sbuffer_destroy(&sbuf);
          break;
        }

        if (!ifb_ptcl_try_send_msg_packet(
                IFB_MSG_TYPE_GAME_STATE_UPD, IFB_NET_CHANNEL_STREAM, &sbuf,
                CLIENT_PEER, CLIENT_HOST, true, true, err, ERR_BUFF_SIZE)) {
          printf("%s send_msg_packet_to_server error: %s\n", NET_SUFFIX, err);
        }
        msgpack_sbuffer_destroy(&sbuf);
        break;
      }
      case (TICK_SLAVE): {
        strct_user_input *data = cmd_msg->data;
        if (data == NULL) {
          fprintf(stderr, "cannot resolve strct_user_input* from message.\n");
          continue;
        }

        handle_cmd_slave_tick(data);
        free(data);
        break;
      }
      default: {
        break;
      }
      }

      if (queue_data != NULL) {
        free(cmd_msg);
      }
    }

    thrd_sleep(&ts, NULL);
  }

  return NULL;
}

static bool try_execute_tick_cmd(uint32_t ticks, strct_user_input user_input,
                                 char *err_buf, size_t err_buf_size) {

  pthread_mutex_lock(&MTX_MY_USER_INFO);
  if (MY_USER_INFO == NULL) {
    snprintf(err_buf, err_buf_size, "MY_USER_INFO is not ready.");
    pthread_mutex_unlock(&MTX_MY_USER_INFO);
    return false;
  }

  ifb_strct_user_info my_ui = *MY_USER_INFO;
  pthread_mutex_unlock(&MTX_MY_USER_INFO);

  //   printf("process input command (ticks: %u, x: %d, y: %d)...\n", ticks,
  //          user_input.x, user_input.y);

  if (my_ui.is_master) {
    // мастер обрабатывает свой инпут и чужой (приходящий из сервера)
    pthread_mutex_lock(&MTX_CURRENT_GAME_STATE);

    if (CURRENT_GAME_STATE == NULL) {
      snprintf(err_buf, err_buf_size, "CURRENT_GAME_STATE is not ready.");
      pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);
      return false;
    }

    ifb_strct_game_state game_state = *CURRENT_GAME_STATE;
    pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);

    game_state.tick = ticks;
    game_state.pos_1.is_exists = true;
    game_state.pos_1.x += user_input.x;
    game_state.pos_1.y += user_input.y;

    // собираем чужие инпуты
    pthread_mutex_lock(&MTX_LAST_PLAYER_INPUTS);
    for (size_t i = 0; i < OTHERS_COUNT; i++) {
      ifb_strct_player_input player_input = LAST_PLAYER_INPUTS[i];
      if (i == 0) {
        game_state.pos_2.x += player_input.x;
        game_state.pos_2.y += player_input.y;
      } else if (i == 1) {
        game_state.pos_3.x += player_input.x;
        game_state.pos_3.y += player_input.y;
      } else if (i == 2) {
        game_state.pos_4.x += player_input.x;
        game_state.pos_4.y += player_input.y;
      }
    }
    pthread_mutex_unlock(&MTX_LAST_PLAYER_INPUTS);

    pthread_mutex_lock(&MTX_CURRENT_GAME_STATE);
    *CURRENT_GAME_STATE = game_state;
    pthread_mutex_unlock(&MTX_CURRENT_GAME_STATE);

    enqueue_cmd(TICK_MC, NULL);
  } else {
    // отправляем свой инпут
    strct_user_input *out_input = malloc(sizeof(strct_user_input));
    *out_input = user_input;
    enqueue_cmd(TICK_SLAVE, out_input);
  }

  return true;
}

static void send_connect_cmd(const char *host, int port) {

  if (host == NULL) {
    printf("ERR -> send_connect_cmd() host is NULL !\n");
    return;
  }

  size_t host_buf_size = ERR_BUFF_SIZE;
  char *host_buf = malloc(host_buf_size);

  if (host_buf == NULL) {
    fprintf(stderr,
            "ERR -> send_connect_cmd() malloc() for host_buf returns NULL !\n");
    return;
  }

  if (!safe_strncpy(host_buf, host, host_buf_size)) {
    fprintf(stderr, "ERR -> send_connect_cmd() safe_strncpy() failed!\n");
    return;
  }

  host_buf[host_buf_size - 1] = '\0';

  strct_msg_connect *msg_connect = malloc(sizeof(strct_msg_connect));
  msg_connect->port = port;
  msg_connect->host = host_buf;

  enqueue_cmd(CONNECT, (void *)msg_connect);
}

static bool is_user_info_ready() {
  pthread_mutex_lock(&MTX_MY_USER_INFO);
  bool result = false;
  if (MY_USER_INFO != NULL) {
    result = true;
  }
  pthread_mutex_unlock(&MTX_MY_USER_INFO);

  return result;
}

static void *tick_loop_thread(void *arg) {
  (void)arg;

  time_t ms = 1000 / FRAME_RATE;
  printf("MS: %ld\n", ms);
  struct timespec ts = ifb_mat_timespec_ms(ms);

  uint64_t total_frames = 0;
  while (atomic_load(&IS_APP_RUNNING)) {

    if (total_frames % FRAME_RATE == 0) {
      // printf("frame ticks: %ld\n", total_frames);
    }

    // TODO: AUTOTICK

    if (is_user_info_ready()) {
      char err_buf[ERR_BUFF_SIZE];
      strct_user_input input = {0};
      uint32_t ticks = (uint32_t)total_frames;
      if (try_execute_tick_cmd(ticks, input, err_buf, ERR_BUFF_SIZE)) {

      } else {
        fprintf(stderr, "ERR -> tick_loop_thread() try_execute_tick_cmd()%s\n",
                err_buf);
      }
    }

    thrd_sleep(&ts, NULL);
    total_frames++;
  }

  return NULL;
}

int main(int argc, char **argv) {

  char *host = NULL;
  if (argc == 1) {
    host = "127.0.0.1";
  } else if (argc == 2) {
    host = argv[1];
  }

  if (enet_initialize() != 0) {
    print_err("enet_initialize failed.");
    exit(EXIT_FAILURE);
  }
  atexit(enet_deinitialize);

  atomic_store(&IS_APP_RUNNING, true);

  // mutexes
  pthread_mutex_init(&MTX_CURRENT_GAME_STATE, NULL);
  pthread_mutex_init(&MTX_MY_USER_INFO, NULL);
  pthread_mutex_init(&MTX_LAST_PLAYER_INPUTS, NULL);

  pthread_mutex_t local_cmds_mutex;
  pthread_mutex_init(&local_cmds_mutex, NULL);

  pthread_cond_t local_cmds_cond;
  pthread_cond_init(&local_cmds_cond, NULL);

  ifb_strct_safe_queue cmds_q = {0};
  cmds_q.mutex = local_cmds_mutex;
  cmds_q.cond = local_cmds_cond;
  LOCAL_COMMANDS_SF_QUEUE = &cmds_q;

  pthread_cond_t incoming_messages_cond;
  pthread_cond_init(&incoming_messages_cond, NULL);

  // end

  pthread_t net_thread;
  pthread_create(&net_thread, NULL, net_bg_thread, NULL);

  pthread_t tick_thread;
  pthread_create(&tick_thread, NULL, tick_loop_thread, NULL);

  // input loop

  uint32_t local_ticks = 0;

  const char *cmd_connect = "connect";
  const char *cmd_req_user_info = "request_ui";
  const char *cmd_connect_n_register = "conreg";
  const char *cmd_tick = "tick";
  const char *cmd_tick_x_pos = "x+1";
  const char *cmd_tick_x_neg = "x-1";
  const char *cmd_tick_y_pos = "y+1";
  const char *cmd_tick_y_neg = "y-1";

  printf("write something to send on server, or type 'exit':\n");
  printf("\t< < type '%s' to connect.\n", cmd_connect);
  printf("\t< < type '%s' to request user_info from server.\n",
         cmd_req_user_info);
  printf("\t< < type '%s' to connect & request user_info.\n",
         cmd_connect_n_register);
  puts("\nticks:\n");
  printf("\t< < type '%s' to update tick.\n", cmd_tick);
  printf("\t< < type '%s' to update tick with x input +1.\n", cmd_tick_x_pos);
  printf("\t< < type '%s' to update tick with x input -1.\n", cmd_tick_x_neg);
  printf("\t< < type '%s' to update tick with y input 1.\n", cmd_tick_y_pos);
  printf("\t< < type '%s' to update tick with y input -1.\n", cmd_tick_y_neg);

  char input_buf[1024];
  while (true) {
    printf("> ");
    if (fgets(input_buf, sizeof(input_buf), stdin) != NULL) {
      // находим символ новой строки, который добавляется fgets() и заменяем его
      // символом конца строки (null-terminate).
      input_buf[strcspn(input_buf, "\n")] = '\0';

      if (strcmp(input_buf, "exit") == 0) {
        printf("goodbye! wait until disconnect...\n");
        break;
      }

      if (strcmp(input_buf, cmd_connect_n_register) == 0) {
        send_connect_cmd(host, PTCL_PORT);
        enqueue_cmd(REQ_USER_INFO, NULL);
        continue;
      }

      if (strcmp(input_buf, cmd_connect) == 0) {
        send_connect_cmd(host, PTCL_PORT);
        continue;
      }

      if (strcmp(input_buf, cmd_req_user_info) == 0) {

        enqueue_cmd(REQ_USER_INFO, NULL);
        continue;
      }

      strct_user_input input = {0};
      bool is_tick_input = false;

      if (strcmp(input_buf, cmd_tick) == 0) {

        is_tick_input = true;
      }

      if (strcmp(input_buf, cmd_tick_x_pos) == 0) {
        input.x = 1;
        is_tick_input = true;
      }
      if (strcmp(input_buf, cmd_tick_x_neg) == 0) {
        input.x = -1;
        is_tick_input = true;
      }
      if (strcmp(input_buf, cmd_tick_y_pos) == 0) {
        input.y = 1;
        is_tick_input = true;
      }
      if (strcmp(input_buf, cmd_tick_y_neg) == 0) {
        input.y = -1;
        is_tick_input = true;
      }

      if (is_tick_input) {
        char err_buf[ERR_BUFF_SIZE];
        if (try_execute_tick_cmd(local_ticks, input, err_buf, ERR_BUFF_SIZE)) {

          local_ticks++;
        } else {
          fprintf(stderr, "try_execute_tick_cmd err: %s\n", err_buf);
        }
        continue;
      }

      printf("unknown command: '%s', try again.\n", input_buf);
      continue;
    }
  }

  // end of app. cleanup

  enqueue_cmd(DISCONNECT_N_FREE, NULL);

  // ждем пока обработается команда дисконекта.
  struct timespec ts = ifb_mat_timespec_ms(1000);
  thrd_sleep(&ts, NULL);

  atomic_store(&IS_APP_RUNNING, false);
  pthread_join(tick_thread, NULL);
  pthread_join(net_thread, NULL);
  pthread_mutex_destroy(&local_cmds_mutex);
  pthread_mutex_destroy(&MTX_CURRENT_GAME_STATE);
  pthread_mutex_destroy(&MTX_MY_USER_INFO);
  pthread_mutex_destroy(&MTX_LAST_PLAYER_INPUTS);

  exit(EXIT_SUCCESS);
}