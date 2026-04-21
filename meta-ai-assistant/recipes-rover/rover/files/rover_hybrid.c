#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// ── CAN IDs (schema) ─────────────────────────────────────────
#define CAN_ID_DRIVE_CMD  0x1F0  // Pi → ESP32
#define CAN_ID_HEARTBEAT  0x110  // Pi → ESP32
#define CAN_ID_TELEMETRY  0x102  // ESP32 → Pi
#define CAN_ID_ESTOP      0x103  // bidirectional
#define CAN_ID_ESP_HB     0x120  // ESP32 → Pi heartbeat
#define CAN_ID_ESP_STATUS 0x121  // ESP32 → Pi status
#define CAN_ID_ESP_TLM    0x122  // ESP32 → Pi telemetry

// ── commands Drive ────────────────────────────────────�
#define CMD_FORWARD  0x01
#define CMD_BACKWARD 0x02
#define CMD_LEFT     0x03
#define CMD_RIGHT    0x04
#define CMD_STOP     0x05

// ── Telemetry flags ───────────────────────────────────────────
#define FLAG_HEARTBEAT_OK 0x01
#define FLAG_ESTOP_ACTIVE 0x02
#define FLAG_MOTORS_ON    0x04
#define FLAG_CMD_VALID    0x08

// ── Timing ────────────────────────────────────────────────────
#define FAST_LOOP_MS       100   // local decision every 100ms
#define AI_QUERY_INTERVAL  30    // Gemini query every 30s
#define HB_INTERVAL        1     // heartbeat every 1s
#define AI_BRIDGE_PORT     5000

// ── High-level AI goals ───────────────────────────────────────
#define GOAL_EXPLORE   0
#define GOAL_AVOID     1
#define GOAL_STOP      2

// ── Global state ─────────────────────────────────────────────
static int      can_sock     = -1;
static uint8_t  seq          = 0;
static uint8_t  hb_counter   = 0;
static uint8_t  estop_active = 0;
static uint8_t  current_goal = GOAL_EXPLORE;
static uint8_t  last_speed   = 150;

// ESP32 telemetry
static uint8_t  tlm_cmd      = 0;
static uint8_t  tlm_speed    = 0;
static uint8_t  tlm_flags    = 0;
static uint8_t  tlm_seq      = 0;

// ESP32 status
static uint8_t  esp_manual   = 1;
static uint8_t  esp_estop    = 0;
static uint8_t  esp_pi_alive = 0;

// Obstacle detection from telemetry
static uint8_t  obstacle_detected = 0;

// Mutex for shared state
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

// ── CAN helpers ───────────────────────────────────────────────
int can_init(const char *ifname) {
    struct sockaddr_can addr;
    struct ifreq ifr;

    can_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_sock < 0) { perror("socket"); return -1; }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(can_sock, SIOCGIFINDEX, &ifr) < 0) { perror("ioctl"); return -1; }

    addr.can_family    = AF_CAN;
    addr.can_ifindex   = ifr.ifr_ifindex;
    if (bind(can_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return -1;
    }

    printf("[CAN] Interface %s initialized\n", ifname);
    return 0;
}

int can_send_drive(uint8_t cmd, uint8_t speed) {
    if (estop_active) return -1;

    struct can_frame frame = {0};
    frame.can_id  = CAN_ID_DRIVE_CMD;
    int8_t pct = 100;  // always use 100% to overcome friction
    int8_t left = 0, right = 0;
    switch(cmd) {
        case 0x01: left =  pct; right =  pct; break; // FORWARD
        case 0x02: left = -pct; right = -pct; break; // BACKWARD
        case 0x03: left = -64;  right =  64;  break; // LEFT
        case 0x04: left =  64;  right = -64;  break; // RIGHT
        default:   left =  0;   right =  0;   break; // STOP
    }
    frame.can_dlc = 8;
    frame.data[0] = (cmd == 0x05) ? 0x00 : 0x01;
    frame.data[1] = 0x01;
    frame.data[2] = (uint8_t)left;
    frame.data[3] = (uint8_t)right;
    frame.data[4] = seq++;
    frame.data[5] = (cmd == 0x05) ? 0x00 : 0x01;
    frame.data[6] = 0;
    frame.data[7] = 0;

    if (write(can_sock, &frame, sizeof(frame)) < 0) {
        perror("can write"); return -1;
    }

    const char *names[] = {"","FWD","BACK","LEFT","RIGHT","STOP"};
    printf("[DRIVE] %s speed=%d seq=%d\n", names[cmd<6?cmd:0], speed, seq-1);
    return 0;
}

int can_send_heartbeat() {
    struct can_frame frame = {0};
    frame.can_id  = CAN_ID_HEARTBEAT;
    frame.can_dlc = 1;
    frame.data[0] = hb_counter++;
    if (write(can_sock, &frame, sizeof(frame)) < 0) return -1;
    return 0;
}

// ── CAN receive thread ────────────────────────────────────────
void *can_recv_thread(void *arg) {
    struct can_frame frame;
    while (1) {
        if (read(can_sock, &frame, sizeof(frame)) < 0) break;

        pthread_mutex_lock(&state_mutex);

        switch (frame.can_id) {
        case CAN_ID_TELEMETRY:
            if (frame.can_dlc >= 4) {
                tlm_cmd   = frame.data[0];
                tlm_speed = frame.data[1];
                tlm_flags = frame.data[2];
                tlm_seq   = frame.data[3];
                if (tlm_flags & FLAG_ESTOP_ACTIVE) estop_active = 1;
            }
            break;

        case CAN_ID_ESTOP:
            if (frame.can_dlc >= 1) {
                estop_active = frame.data[0];
                printf("[SAFETY] E-Stop from ESP32: %s\n",
                       estop_active ? "ON" : "OFF");
            }
            break;

        case CAN_ID_ESP_HB:
            if (frame.can_dlc >= 4) {
                esp_manual   = frame.data[1];
                esp_estop    = frame.data[2];
                esp_pi_alive = frame.data[3];
            }
            break;

        case CAN_ID_ESP_STATUS:
            if (frame.can_dlc >= 3) {
                esp_manual   = frame.data[0];
                esp_estop    = frame.data[1];
                esp_pi_alive = frame.data[2];
            }
            break;

        case CAN_ID_ESP_TLM:
            // Use motor feedback to detect if rover is stuck
            if (frame.can_dlc >= 4) {
                int8_t left  = (int8_t)frame.data[0];
                int8_t right = (int8_t)frame.data[1];
                // If commanded to move but motors report 0, might be stuck
                if (tlm_cmd == CMD_FORWARD && left == 0 && right == 0)
                    obstacle_detected = 1;
                else
                    obstacle_detected = 0;
            }
            break;

        default:
            break;
        }

        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}

// ── Heartbeat thread ──────────────────────────────────────────
void *heartbeat_thread(void *arg) {
    while (1) {
        can_send_heartbeat();
        sleep(HB_INTERVAL);
    }
    return NULL;
}

// ── Local decision algorithm (fast loop) ─────────────────────
uint8_t local_decision() {
    pthread_mutex_lock(&state_mutex);
    uint8_t goal     = current_goal;
    uint8_t obstacle = obstacle_detected;
    uint8_t manual   = esp_manual;
    pthread_mutex_unlock(&state_mutex);

    // Safety: if ESP32 in manual mode, don't send commands
    if (manual) {
        printf("[LOCAL] ESP32 in manual mode - waiting\n");
        return CMD_STOP;
    }

    // If obstacle detected locally, override goal
    if (obstacle) {
        printf("[LOCAL] Obstacle detected! Turning...\n");
        return CMD_LEFT;
    }

    // Follow AI goal
    switch (goal) {
    case GOAL_EXPLORE: return CMD_FORWARD;
    case GOAL_AVOID:   return CMD_RIGHT;
    case GOAL_STOP:    return CMD_STOP;
    default:           return CMD_STOP;
    }
}

// ── Fast loop thread (100ms) ──────────────────────────────────
void *fast_loop_thread(void *arg) {
    struct timespec ts = {0, FAST_LOOP_MS * 1000000L};

    sleep(3);  // Wait for system to settle

    while (1) {
        // Check if manual mode is active (UI sets this file)
        FILE *f = fopen("/tmp/rover_manual", "r");
        if (f) {
            fclose(f);
            nanosleep(&ts, NULL);
            continue;
        }

        if (!estop_active) {
            uint8_t cmd = local_decision();
            can_send_drive(cmd, last_speed);
        }
        nanosleep(&ts, NULL);
    }
    return NULL;
}

// ── AI call bridge ─────────�
uint8_t ask_ai(const char *context) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return GOAL_EXPLORE;

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(AI_BRIDGE_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        printf("[AI] Bridge not available\n");
        return GOAL_EXPLORE;
    }

    char body[512];
    snprintf(body, sizeof(body),
        "{\"question\":\"You are guiding a rover. %s "
        "Reply with ONLY one word: EXPLORE, AVOID, or STOP.\"}",
        context);

    char request[1024];
    snprintf(request, sizeof(request),
        "POST /ask HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n%s",
        strlen(body), body);

    send(sock, request, strlen(request), 0);

    char response[4096] = {0};
    char buf[1024];
    int n, total = 0;
    while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0 &&
           total < (int)sizeof(response)-1) {
        buf[n] = 0;
        strncat(response, buf, sizeof(response)-total-1);
        total += n;
    }
    close(sock);

    printf("[AI] Goal update: %.200s\n", response);

    if (strstr(response, "EXPLORE")) return GOAL_EXPLORE;
    if (strstr(response, "AVOID"))   return GOAL_AVOID;
    if (strstr(response, "STOP"))    return GOAL_STOP;
    return GOAL_EXPLORE;
}

// ── AI slow loop thread (30s) ─────────────────────────────────
void *ai_slow_loop_thread(void *arg) {
    sleep(10);  // Wait before first query

    while (1) {
        char context[512];

        pthread_mutex_lock(&state_mutex);
        uint8_t flags = tlm_flags;
        uint8_t speed = tlm_speed;
        pthread_mutex_unlock(&state_mutex);

        snprintf(context, sizeof(context),
            "Based on the camera feed, set the rover navigation goal. "
            "Is the path clear for exploration? Any obstacles? "
            "Motor flags=0x%02X speed=%d.",
            flags, speed);

        uint8_t new_goal = ask_ai(context);

        pthread_mutex_lock(&state_mutex);
        current_goal = new_goal;
        pthread_mutex_unlock(&state_mutex);

        const char *goal_names[] = {"EXPLORE","AVOID","STOP"};
        printf("[AI] New goal: %s\n", goal_names[new_goal]);

        sleep(AI_QUERY_INTERVAL);
    }
    return NULL;
}

// ── Main ──────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    const char *ifname = (argc > 1) ? argv[1] : "can0";

    if (can_init(ifname) < 0) return 1;

    printf("[Rover] Hybrid AI Rover Controller started\n");
    printf("[Rover] Fast loop: %dms | AI loop: %ds\n",
           FAST_LOOP_MS, AI_QUERY_INTERVAL);

    can_send_drive(CMD_STOP, 0);
    can_send_heartbeat();

    pthread_t recv_t, hb_t, fast_t, ai_t;
    pthread_create(&recv_t,  NULL, can_recv_thread,      NULL);
    pthread_create(&hb_t,    NULL, heartbeat_thread,     NULL);
    pthread_create(&fast_t,  NULL, fast_loop_thread,     NULL);
    pthread_create(&ai_t,    NULL, ai_slow_loop_thread,  NULL);

    pthread_join(recv_t, NULL);
    return 0;
}
