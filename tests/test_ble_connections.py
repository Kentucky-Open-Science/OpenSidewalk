#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise real BLE bridge callbacks; only Zephyr/radio boundaries are faked."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
headers = '''
#ifndef BLE_TEST_ZEPHYR_H
#define BLE_TEST_ZEPHYR_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define CONFIG_BT_MAX_CONN 4
#define CONFIG_BT_DEVICE_NAME "OpenSidewalk"
#define ARG_UNUSED(x) ((void)(x))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define K_NO_WAIT 0
#define K_SECONDS(n) (n)
#define LOG_LEVEL_INF 0
#define LOG_MODULE_REGISTER(...)
static void test_log(const char *fmt,...) { (void)fmt; }
#define LOG_INF(...) test_log(__VA_ARGS__)
#define LOG_WRN(...) test_log(__VA_ARGS__)
#define BT_LE_AD_GENERAL 2
#define BT_LE_AD_NO_BREDR 4
#define BT_DATA_FLAGS 1
#define BT_DATA_UUID16_ALL 3
#define BT_DATA_NAME_COMPLETE 9
#define BT_UUID_DIS_VAL 0x180a
#define BT_UUID_16_ENCODE(x) 0
#define BT_DATA_BYTES(...) {0}
#define BT_DATA(...) {0}
#define BT_LE_ADV_CONN_FAST_1 0
#define BT_ADDR_LE_STR_LEN 40
struct bt_data { int unused; };
typedef struct { uint8_t val[6]; } bt_addr_t;
typedef struct { uint8_t type; bt_addr_t a; } bt_addr_le_t;
struct bt_conn { bt_addr_le_t peer; uint8_t index; };
struct bt_conn_cb {
    void (*connected)(struct bt_conn *, uint8_t);
    void (*disconnected)(struct bt_conn *, uint8_t);
    void (*recycled)(void);
};
#define BT_CONN_CB_DEFINE(name) static struct bt_conn_cb name
static const bt_addr_le_t *bt_conn_get_dst(struct bt_conn *c) { return &c->peer; }
static uint8_t bt_conn_index(struct bt_conn *c) { return c->index; }
static void bt_addr_le_to_str(const bt_addr_le_t *a, char *out, size_t n) {
    (void)a; snprintf(out,n,"test peer");
}
typedef int atomic_t;
static int atomic_get(const atomic_t *a) { return *a; }
static void atomic_set(atomic_t *a,int v) { *a=v; }
static void atomic_clear(atomic_t *a) { *a=0; }
static int atomic_inc(atomic_t *a) { return (*a)++; }
static int atomic_dec(atomic_t *a) { return (*a)--; }
static void atomic_set_bit(atomic_t *a,int bit) { *a |= 1u << bit; }
static void atomic_clear_bit(atomic_t *a,int bit) { *a &= ~(1u << bit); }
struct k_spinlock { int unused; };
typedef int k_spinlock_key_t;
static k_spinlock_key_t k_spin_lock(struct k_spinlock *s) { (void)s; return 0; }
static void k_spin_unlock(struct k_spinlock *s,k_spinlock_key_t k) { (void)s; (void)k; }
struct k_work { void (*handler)(struct k_work *); };
struct k_work_delayable { struct k_work work; bool scheduled; };
#define K_WORK_DELAYABLE_DEFINE(name,fn) static struct k_work_delayable name = {{fn},false}
static int k_work_reschedule(struct k_work_delayable *w,int delay) {
    (void)delay; w->scheduled=true; return 1;
}
static bool controller_advertising;
static unsigned int controller_connections;
static unsigned int advertiser_starts;
static int bt_enable(void *cb) { (void)cb; return 0; }
static int bt_le_adv_start(int params,const struct bt_data *ad,size_t n,
                          const struct bt_data *sd,size_t m) {
    (void)params; (void)ad; (void)n; (void)sd; (void)m;
    if (controller_connections >= CONFIG_BT_MAX_CONN) return -ENOMEM;
    if (controller_advertising) return -EALREADY;
    controller_advertising=true; advertiser_starts++; return 0;
}
struct shell { char output[512]; };
static void shell_print(const struct shell *sh,const char *fmt,...) {
    struct shell *s=(struct shell *)sh; va_list ap; va_start(ap,fmt);
    size_t n=strlen(s->output);
    vsnprintf(s->output+n,sizeof(s->output)-n,fmt,ap); va_end(ap);
}
#define SHELL_STATIC_SUBCMD_SET_CREATE(...)
#define SHELL_CMD_REGISTER(...)
#endif
'''

test = r'''
#include "ble_bridge.c"
static void run_advertiser(void) {
    if (advertise_work.scheduled) {
        advertise_work.scheduled=false;
        advertise_work.work.handler(&advertise_work.work);
    }
}
static void connect_peer(struct bt_conn *peer) {
    assert(controller_advertising);
    controller_advertising=false; /* Controller stops the set on connection. */
    controller_connections++;
    bridge_connections.connected(peer,0);
    run_advertiser();
}
int main(void) {
    struct bt_conn peers[5]={0};
    uint8_t payload[BLE_EVENT_SIZE];
    for (unsigned int i=0;i<5;i++) {
        peers[i].peer.type=1;
        peers[i].peer.a.val[0]=i+1;
        peers[i].peer.a.val[5]=0xc0;
        peers[i].index=i<4 ? i : 1; /* Fifth peer reuses the second slot. */
    }
    assert(ble_bridge_init()==0);
    run_advertiser();
    connect_peer(&peers[0]);
    assert(controller_advertising && "Must accept another peer while first remains connected");
    connect_peer(&peers[1]);
    connect_peer(&peers[2]);
    assert(controller_advertising);
    connect_peer(&peers[3]);
    assert(!controller_advertising && controller_connections==4);
    struct shell sh={0};
    cmd_status(&sh,0,NULL);
    assert(strstr(sh.output,"connections: 4/4"));
    for (unsigned int i=0;i<4;i++) {
        assert(ble_bridge_pending(payload));
        assert(payload[3]==i+1 && payload[5]==0xc0 && payload[10]==i+1);
        ble_bridge_complete(true);
    }
    assert(!ble_bridge_pending(payload));
    /* Disconnect must preserve the other three peers and wait for a free slot. */
    controller_connections--;
    bridge_connections.disconnected(&peers[1],0x13);
    run_advertiser();
    assert(!controller_advertising);
    bridge_connections.recycled();
    run_advertiser();
    assert(controller_advertising);
    connect_peer(&peers[4]);
    assert(!controller_advertising);
    assert(ble_bridge_pending(payload) && payload[3]==5 && payload[10]==5);
    ble_bridge_complete(true);
    /* Failed establishment followed by disconnect must preserve other peers.
     * Zephyr can emit both callbacks for an immediately terminated link.
     */
    controller_connections--;
    bridge_connections.disconnected(&peers[2],0x13);
    bridge_connections.recycled();
    run_advertiser();
    controller_advertising=false;
    bridge_connections.connected(&peers[2],0x3e);
    bridge_connections.disconnected(&peers[2],0x3e);
    bridge_connections.recycled();
    run_advertiser();
    memset(&sh,0,sizeof(sh)); cmd_status(&sh,0,NULL);
    assert(strstr(sh.output,"connections: 3/4"));
    assert(!ble_bridge_pending(payload));
    assert(controller_advertising && advertiser_starts==7);
    for (unsigned int i=0;i<5;i++) {
        if (i==1 || i==2) continue;
        controller_connections--;
        bridge_connections.disconnected(&peers[i],0x13);
        bridge_connections.recycled();
        run_advertiser();
    }
    memset(&sh,0,sizeof(sh)); cmd_status(&sh,0,NULL);
    assert(strstr(sh.output,"connections: 0/4"));
    assert(controller_advertising && !ble_bridge_pending(payload));
    puts("PASS: four concurrent BLE peers, capacity/recycled advertising, distinct TTN events and failed setup.");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='opensidewalk-multi-ble-') as temp:
    folder = Path(temp)
    (folder/'stub.h').write_text(headers)
    for name in ('kernel.h','bluetooth/bluetooth.h','bluetooth/conn.h',
                 'bluetooth/uuid.h','logging/log.h','shell/shell.h','sys/atomic.h'):
        p = folder/'zephyr'/name
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text('#include "stub.h"\n')
    (folder/'test.c').write_text(test)
    exe=folder/'test'
    subprocess.run(['clang','-std=c17','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-Wno-unused-const-variable',
                    '-I'+str(folder),'-Isrc',str(folder/'test.c'),
                    'src/ble_event.c','-o',str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],check=True)
