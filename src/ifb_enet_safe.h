#pragma once

#include <stdbool.h>
#include <enet/enet.h>
#include <enet/protocol.h>
#include <stdio.h>


// https://github.com/lsalzman/enet/blob/master/peer.c
bool try_enet_peer_send(ENetPeer* peer, enet_uint32 channel_id, ENetPacket* packet, char* err_buf, size_t err_buf_size) {

    if (peer == NULL) {
        snprintf(err_buf, err_buf_size, "try_enet_peer_send() peer is NULL");
        return false;
    }

    if (packet == NULL) {
        snprintf(err_buf, err_buf_size, "try_enet_peer_send() packet is NULL");
        return false;
    }

    if (peer->state != ENET_PEER_STATE_CONNECTED) {
        snprintf(err_buf, err_buf_size, "try_enet_peer_send() peer state is not connected. Actual: %d", peer->state);
        return false;
    }

    if(channel_id >= peer->channelCount) {
        snprintf(err_buf, err_buf_size, "try_enet_peer_send() channel_id is larger than peer->channelCount");
        return false;
    }

    if (packet->dataLength > peer->host->maximumPacketSize) {
        snprintf(err_buf, err_buf_size, "try_enet_peer_send() cpacket->dataLength is larger than host->maximumPacketSize (%lu / %lu)", packet->dataLength, peer->host->maximumPacketSize);
        return false;
    }

    int res = enet_peer_send(peer, channel_id, packet);

    if (res < 0) {
        snprintf(err_buf, err_buf_size, "try_enet_peer_send() uknown error.");
        return false;
    }

    return true;
}