// Cartesian kinematics stepper pulse time generation
//
// Copyright (C) 2018-2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <math.h>
#include <stdlib.h> // malloc
#include <string.h> // memset
#include "compiler.h" // __visible
#include "itersolve.h" // struct stepper_kinematics
#include "pyhelper.h" // errorf
#include "trapq.h" // move_get_coord

static double
cart_stepper_calc_position(struct stepper_kinematics *sk, struct move *m, const double move_time, const char axis)
{
    return move_get_coord(m, move_time).axis[axis - 'x'];
    const double move_dist_axis = move_get_distance(m, move_time) * m->axes_r.axis[axis - 'x'];
    const double effective_move_dist_axis = fmax(move_dist_axis - m->backlash_axes.axis[axis - 'x'], .0);
    return move_get_coord_by_dist(m, effective_move_dist_axis).axis[axis - 'x'];
}

static double
cart_stepper_x_calc_position(struct stepper_kinematics *sk, struct move *m
                             , double move_time)
{
    return cart_stepper_calc_position(sk, m, move_time, 'x');
}

static double
cart_stepper_y_calc_position(struct stepper_kinematics *sk, struct move *m
                             , double move_time)
{
    return cart_stepper_calc_position(sk, m, move_time, 'y');
}

static double
cart_stepper_z_calc_position(struct stepper_kinematics *sk, struct move *m
                             , double move_time)
{
    return cart_stepper_calc_position(sk, m, move_time, 'z');
}

struct stepper_kinematics * __visible
cartesian_stepper_alloc(char axis)
{
    struct stepper_kinematics *sk = malloc(sizeof(*sk));
    memset(sk, 0, sizeof(*sk));
    if (axis == 'x') {
        sk->calc_position_cb = cart_stepper_x_calc_position;
        sk->active_flags = AF_X;
    } else if (axis == 'y') {
        sk->calc_position_cb = cart_stepper_y_calc_position;
        sk->active_flags = AF_Y;
    } else if (axis == 'z') {
        sk->calc_position_cb = cart_stepper_z_calc_position;
        sk->active_flags = AF_Z;
    }
    return sk;
}

static double
cart_reverse_stepper_x_calc_position(struct stepper_kinematics *sk
                             , struct move *m, double move_time)
{
    return -cart_stepper_calc_position(sk, m, move_time, 'x');
}

static double
cart_reverse_stepper_y_calc_position(struct stepper_kinematics *sk
                             , struct move *m, double move_time)
{
    return -cart_stepper_calc_position(sk, m, move_time, 'y');
}

static double
cart_reverse_stepper_z_calc_position(struct stepper_kinematics *sk
                             , struct move *m, double move_time)
{
    return -cart_stepper_calc_position(sk, m, move_time, 'z');
}

struct stepper_kinematics * __visible
cartesian_reverse_stepper_alloc(char axis)
{
    struct stepper_kinematics *sk = malloc(sizeof(*sk));
    memset(sk, 0, sizeof(*sk));
    if (axis == 'x') {
        sk->calc_position_cb = cart_reverse_stepper_x_calc_position;
        sk->active_flags = AF_X;
    } else if (axis == 'y') {
        sk->calc_position_cb = cart_reverse_stepper_y_calc_position;
        sk->active_flags = AF_Y;
    } else if (axis == 'z') {
        sk->calc_position_cb = cart_reverse_stepper_z_calc_position;
        sk->active_flags = AF_Z;
    }
    return sk;
}
