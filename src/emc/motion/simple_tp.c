/********************************************************************
* Description: simple_tp.c
*   A simple single axis trajectory planner.  See simple_tp.h for API.
*
* Author: jmkasunich
* License: GPL Version 2
* Created on:
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "simple_tp.h"
#include "rtapi_math.h"
#include "posemath.h"

// to disable DP(): #define TRACE 0
#define TRACE 0
#include "dptrace.h"
#if (TRACE!=0)
// FILE *dptrace = fopen("dptrace.log","w");
static FILE *dptrace = 0;
static uint32_t dt = 0;
#endif

/*
 Continuous form
 PT = P0 + V0T + 1/2A0T2 + 1/6JT3
 VT = V0 + A0T + 1/2 JT2
 AT = A0 + JT

 Discrete time form (讓 T 成為 1, 那麼 T^2 == 1, T^3 == 1)
 PT = PT + VT + 1/2AT + 1/6J
 VT = VT + AT + 1/2JT
 AT = AT + JT
 */

void simple_tp_update(simple_tp_t *tp, double period)
{
    double max_dv, tiny_dp, pos_err, vel_req;
    double max_da;
    double next_acc;

    tp->active = 0;
    /* compute max change in velocity per servo period */
    if (tp->position_mode) {
        max_dv = tp->max_acc * period;
    } else {
        // s-curve for non-positioning motion only
        max_da = tp->max_jerk * period;
        next_acc = fabs(tp->curr_acc) + max_da;
        if (next_acc > tp->max_acc) {
            next_acc = tp->max_acc;
        }
        max_dv = next_acc * period;
    }
        
    /* compute a tiny position range, to be treated as zero */
    tiny_dp = max_dv * period * 0.001;
    /* calculate desired velocity */
    if (tp->enable) {
        /* accurately positioning at "tp->pos_cmd" */
        /* planner enabled, request a velocity that tends to drive
           pos_err to zero, but allows for stopping without position
           overshoot */
        pos_err = tp->pos_cmd - tp->curr_pos;
        /* positive and negative errors require some sign flipping to
           avoid sqrt(negative) */
        if (pos_err > tiny_dp) {
            vel_req = -max_dv +
                       sqrt(2.0 * tp->max_acc * pos_err + max_dv * max_dv);
            /* mark planner as active */
            tp->active = 1;
        } else if (pos_err < -tiny_dp) {
            vel_req =  max_dv -
                       sqrt(-2.0 * tp->max_acc * pos_err + max_dv * max_dv);
            /* mark planner as active */
            tp->active = 1;
        } else {
            /* within 'tiny_dp' of desired pos, no need to move */
            vel_req = 0.0;
        }
    } else {
	/* planner disabled, request zero velocity */
	vel_req = 0.0;
	/* and set command to present position to avoid movement when
	   next enabled */
	tp->pos_cmd = tp->curr_pos;
    }
    /* limit velocity request */
    if (vel_req > tp->max_vel) {
        vel_req = tp->max_vel;
    } else if (vel_req < -tp->max_vel) {
	vel_req = -tp->max_vel;
    }
    /* ramp velocity toward request at accel limit */
    if (vel_req > tp->curr_vel + max_dv) {
	tp->curr_vel += max_dv;
    } else if (vel_req < tp->curr_vel - max_dv) {
	tp->curr_vel -= max_dv;
    } else {
	tp->curr_vel = vel_req;
    }
    /* check for still moving */
    if (tp->curr_vel != 0.0) {
	/* yes, mark planner active */
	tp->active = 1;
    }
    /* integrate velocity to get new position */
    tp->curr_pos += tp->curr_vel * period;




#if (TRACE!=0)
    if (dptrace == 0) {
        dptrace = fopen("simple_tp.log","w");
        dt = 0;
        DPS("%11s  %15s%15s%15s%15s%15s%15s%7s%7s ",
             "#dt",  "pos_cmd", "curr_pos", "curr_vel", "curr_acc", "vel_req", "max_vel", "enable", "active"
           );
    }

    if ((dt % 5) == 3) {        // check for j3 of 5-joints
        DPS("\n%11u ", dt >> 2);    
    }
    DPS(" %15.7f%15.7f%15.7f%15.7f%15.7f%15.7f%7d%7d ",
          tp->pos_cmd, tp->curr_pos, tp->curr_vel, tp->curr_acc, vel_req, tp->max_vel, tp->enable, tp->active
       );
    dt += 1;
#endif

}
