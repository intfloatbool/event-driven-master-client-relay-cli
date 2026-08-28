# Event-Driven Master-Client Relay (CLI, C)

[**RUS Version**](./README.md)

A small educational **networked CLI project written in pure C**, demonstrating an **event-driven model** of client interaction through a relay server with **Master-Client Authority**.

The project was created as a practical exercise in:
- network programming in C,
- working with ENet (UDP + reliable delivery),
- message serialization and deserialization using MsgPack,
- multithreading and synchronization,
- designing client ↔ server architecture without a real-time loop.

---

## 🎯 Project Idea

![](./screens/server_master_slave_slave_cli.png)

- There is a **relay server** that **contains no game logic**
- The first connected client becomes the **Master**
- The Master:
  - stores the world state
  - processes input from other clients
  - broadcasts the updated state to all clients
- Other clients (**Slaves**):
  - send only their input commands
  - apply the state received from the Master

The model is **not real-time**, but **event-driven**:
- the world is updated via the `tick` command
- ideal for learning, debugging, and understanding networking patterns

---

**Key principles:**
- the server does not interpret data
- the server only relays messages
- all logic is concentrated in the Master client
- a single shared message protocol for both client and server

---

## 📂 Project Structure

```

.
├── Makefile
├── README.md
└── src
    ├── client.c              # CLI client (Master / Slave)
    ├── server.c              # Relay server
    ├── protocol_msg.h        # Shared network protocol
    ├── help_func/            # Helper functions
    └── thread_safe/          # Thread-safe structures

````

---

## 🔌 Technologies Used

- **C (C17)**
- **[ENet](https://github.com/lsalzman/enet)** — UDP networking library with reliable delivery
- **[MsgPack-C](https://github.com/msgpack/msgpack-c)** — binary message serialization
- **POSIX Threads (pthreads)** — multithreading
- **Linux / WSL2**

The project targets Unix-like systems.

---

## 📡 Message Protocol

All messages are defined in `protocol_msg.h`.

Examples of message types:
- client registration
- Master assignment
- client input (intention to change state)
- world state snapshot

Serialization is performed using **MsgPack**, which:
- removes the need for manual framing
- makes the protocol extensible
- allows using identical structures on both client and server

---

## 🧵 Threads and Synchronization

- The ENet networking loop runs in a dedicated thread
- CLI input is handled independently
- Inter-thread communication is implemented via thread-safe queues
- Race conditions when working with ENet are eliminated

---

## ▶️ Build

### Dependencies

Install required libraries:
```bash
sudo apt install libenet-dev libmsgpack-dev
````

### Build

```bash
make
```

This will produce:

* `server`
* `client`

---

## 🚀 Run

### Server

```bash
./server
```

### Clients

```bash
./client <server_ip>
```

You can run:

* multiple clients on the same machine
* clients on different machines within the same LAN / Wi-Fi network

---

## 🕹️ Controls (CLI)

Example client commands:

```
tick
x+
x-
y+
y-
```

* `tick` — triggers a world update (Master only)
* other commands send input to the Master client

---

## 📚 Purpose of This Project

This project was created **not as a game**, but as:

* an educational sandbox for network programming
* a foundation for transitioning to:

  * real-time synchronization
  * prediction / reconciliation
  * an authoritative server model
* source material for articles and educational content

---
