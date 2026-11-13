#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile real shell handlers with a deterministic autojoin interleaving at gate acquisition."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/main.c').read_text()
def function(name):
    start = source.index('static int ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#define ARG_UNUSED(x) (void)(x)
#define K_NO_WAIT 0
struct shell { int unused; };
struct node_config {
 uint8_t version, provisioned, subband, reserved;
 uint8_t dev_eui[8], join_eui[8], app_key[16];
 uint32_t last_nonce;
};
enum job_kind { JOB_JOIN, JOB_SEND };
struct job { enum job_kind kind; };
static struct node_config config;
static int ready, joined, busy, jobs, queued;
static int interleave; /* 1: failed autojoin; 2: accepted autojoin */
static int atomic_get(int *v) { return *v; }
static void atomic_clear(int *v) { *v=0; }
static bool atomic_cas(int *v, int old, int value) {
 if (interleave) {
  /* A main-thread join completes immediately before the shell acquires the gate. */
  config.last_nonce++;
  if (interleave==2) joined=1;
  interleave=0;
 }
 if (*v!=old) return false;
 *v=value; return true;
}
static struct node_config config_snapshot(void) { return config; }
static int save_config(const struct node_config *v) { config=*v; return 0; }
static size_t hex2bin(const char *in, size_t n, uint8_t *out, size_t size) {
 (void)in; assert(n==size*2); memset(out,0,size); return size;
}
#define shell_error(...) ((void)0)
#define shell_print(...) ((void)0)
static int k_msgq_put(int *q, struct job *j, int timeout) {
 (void)q; (void)j; (void)timeout; queued++; return 0;
}
'''
suffix = r'''
static void reset_case(void) {
 memset(&config,0,sizeof(config));
 config.provisioned=1; config.subband=2; config.last_nonce=40;
 ready=1; joined=busy=queued=interleave=0;
}
int main(void) {
 struct shell sh={0};
 char *subband[]={"subband","3"};
 char *provision[]={"provision","0000000000000000","0000000000000000","00000000000000000000000000000000"};
 char *join[]={"join"};
 reset_case(); interleave=1;
 assert(cmd_subband(&sh,2,subband)==0);
 assert(config.last_nonce==41); /* A stale pre-gate snapshot would roll back to 40. */
 assert(config.subband==3 && busy==0);
 reset_case(); interleave=1;
 assert(cmd_provision(&sh,4,provision)==0);
 assert(config.last_nonce==41 && busy==0);
 reset_case(); interleave=2;
 assert(cmd_subband(&sh,2,subband)==-EBUSY);
 assert(config.subband==2 && config.last_nonce==41 && busy==0);
 reset_case(); interleave=2;
 assert(cmd_provision(&sh,4,provision)==-EBUSY);
 assert(config.last_nonce==41 && busy==0);
 reset_case(); interleave=2;
 assert(cmd_queue(&sh,1,join)==-EALREADY);
 assert(queued==0 && busy==0);
 reset_case(); config.provisioned=0;
 assert(cmd_queue(&sh,1,join)==-EINVAL && busy==0);
 reset_case(); busy=1;
 assert(cmd_queue(&sh,1,join)==-EBUSY && busy==1);
 reset_case();
 assert(cmd_queue(&sh,1,join)==0 && queued==1 && busy==1);
 puts("PASS: real shell handlers preserve nonce and recheck state after autojoin interleavings.");
}
'''
with tempfile.TemporaryDirectory(prefix='kg100s-control-races-') as directory:
    test = Path(directory)/'test.c'
    test.write_text(prefix + '\n'.join(function(n) for n in ('begin_config','cmd_provision','cmd_subband','cmd_queue')) + suffix)
    exe = Path(directory)/'test'
    subprocess.run(['clang','-std=c17','-Wall','-Wextra','-Wno-unused-parameter',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
