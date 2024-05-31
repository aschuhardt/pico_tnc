/*
Copyright (c) 2021, JN1DFF & Addison Schuhardt, W0ADY
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, 
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution.
* Neither the name of the <organization> nor the names of its contributors 
  may be used to endorse or promote products derived from this software 
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "tnc.h"
#include "ax25.h"

uint32_t __tnc_time;

tnc_t tnc[PORT_N];

enum STATE_CALLSIGN {
    CALL = 0,
    HYPHEN,
    SSID1,
    SSID2,
    SPACE,
    END,
};

param_t param = {
    .mycall = { 0, 0, },
    .unproto = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, },
    .myalias = { 0, 0 },
    .btext = ""
};

static uint8_t *read_call(uint8_t *buf, callsign_t *c)
{
    callsign_t cs;
    int i, j;
    int state = CALL;
    bool error = false;

    cs.call[0] = '\0';
    for (i = 1; i < 6; i++) cs.call[i] = ' ';
    cs.ssid = 0;

    // callsign
    j = 0;
    for (i = 0; buf[i] && state != END; i++) {
        int ch = buf[i];

        switch (state) {

            case CALL:
                if (isalnum(ch)) {
                    cs.call[j++] = toupper(ch);
                    if (j >= 6) state = HYPHEN;
                    break;
                } else if (ch == '-') {
                    state = SSID1;
                    break;
                } else if (ch != ' ') {
                    error = true;
                }
                state = END;
                break;

            case HYPHEN:
                if (ch == '-') {
                    state = SSID1;
                    break;
                }
                if (ch != ' ') {
                    error = true;
                }
                state = END;
                break;

            case SSID1:
                if (isdigit(ch)) {
                    cs.ssid = ch - '0';
                    state = SSID2;
                    break;
                }
                error = true;
                state = END;
                break;

            case SSID2:
                if (isdigit(ch)) {
                    cs.ssid *= 10;
                    cs.ssid += ch - '0';
                    state = SPACE;
                    break;
                }
                /* FALLTHROUGH */

            case SPACE:
                if (ch != ' ') error = true;
                state = END;
        }
    }

    if (cs.ssid > 15) error = true;

    if (error) return NULL;

    memcpy(c, &cs, sizeof(cs));

    return &buf[i];
}

void tnc_init(void (*ptt_on)(void), void (*ptt_off)(void))
{
    // filter initialization
    // LPF
    static const filter_param_t flt_lpf = {
        .size = FIR_LPF_N,
        .sampling_freq = SAMPLING_RATE,
        .pass_freq = 0,
        .cutoff_freq = 1200,
    };
    int16_t *lpf_an, *bpf_an;

    lpf_an = filter_coeff(&flt_lpf);

    // BPF
    static const filter_param_t flt_bpf = {
        .size = FIR_BPF_N,
        .sampling_freq = SAMPLING_RATE,
        .pass_freq = 900,
        .cutoff_freq = 2500,
    };

    bpf_an = filter_coeff(&flt_bpf);

    // PORT initialization
    for (int i = 0; i < PORT_N; i++) {
        tnc_t *tp = &tnc[i];

        // receive
        tp->port = i;
        tp->state = FLAG;
        filter_init(&tp->lpf, lpf_an, FIR_LPF_N);
        filter_init(&tp->bpf, bpf_an, FIR_BPF_N);

        // send queue
        queue_init(&tp->send_queue, sizeof(uint8_t), SEND_QUEUE_LEN);
        tp->send_state = SP_IDLE;
        tp->ptt_on = ptt_on;
        tp->ptt_off = ptt_off;

        tp->cdt = 0;
    }
}

bool set_btext(uint8_t * buf, int len)
{
    len = MIN(len, BTEXT_LEN);
    memcpy(&param.btext[0], buf, len);
    param.btext[len] = '\0';
}

void clear_unproto() {
    for (int i = 0; i < UNPROTO_N; ++i)
        param.unproto[i].call[0] = '\0';
}

bool set_unproto(int index, uint8_t * buf, int len) {
    if (buf == NULL || buf[0] == '\0')
        return false;

    return read_call(buf, &param.unproto[index]) != NULL;
}

bool set_mycall(uint8_t * buf, int len) {
    if (buf == NULL || buf[0] == '\0')
        return false;

    return read_call(buf, &param.mycall) != NULL;
}
