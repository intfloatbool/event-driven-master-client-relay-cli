## При подключении второго клиента, вызов `conreg` бросил segfault
( **FIXED** )

> 	CMD CONNECT: argz ( ./client.out, 7777 )
trying to connect host: ./client.out:7777
FATAL: enet_address_set_host() failed.
 ...	CMD REQ_USER_INFO
Segmentation fault (core dumped)

Пофикшено проверкой:
> ```c
// client.c handle_cmd_connect_to_host()
check_host_error()
> 


## Когда подключены N игроков, у мастер-клиента идет спам ошибок:

> ```bash
> <<<NET_THREAD>>>  send_msg_packet_to_server error: enet_peer_send failed.
> ````
