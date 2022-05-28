# Code for handling the kinematics of cartesian robots
#
# Copyright (C) 2016-2021  Kevin O'Connor <kevin@koconnor.net>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

import logging
import stepper
import traceback
import itertools


class CartKinematics:
    def __init__(self, toolhead, config):
        self.printer = config.get_printer()
        # Setup axis rails
        self.dual_carriage_axis = None
        self.dual_carriage_rails = []
        self.rails = [stepper.LookupMultiRail(config.getsection('stepper_' + n))
                      for n in 'xyz']
        for rail, axis in zip(self.rails, 'xyz'):
            rail.setup_itersolve('cartesian_stepper_alloc', axis.encode())
        for s in self.get_steppers():
            s.set_trapq(toolhead.get_trapq())
            toolhead.register_step_generator(s.generate_steps)
        self.printer.register_event_handler("stepper_enable:motor_off",
                                            self._motor_off)
        # Setup boundary checks
        max_velocity, max_accel = toolhead.get_max_velocity()
        self.max_z_velocity = config.getfloat('max_z_velocity', max_velocity,
                                              above=0., maxval=max_velocity)
        self.max_z_accel = config.getfloat('max_z_accel', max_accel,
                                           above=0., maxval=max_accel)
        self.limits = [(1.0, -1.0)] * 3
        ranges = [r.get_range() for r in self.rails]
        self.axes_min = toolhead.Coord(*[r[0] for r in ranges], e=0.)
        self.axes_max = toolhead.Coord(*[r[1] for r in ranges], e=0.)
        # Check for dual carriage support
        if config.has_section('dual_carriage'):
            dc_config = config.getsection('dual_carriage')
            dc_axis = dc_config.getchoice('axis', {'x': 'x', 'y': 'y'})
            self.dual_carriage_axis = {'x': 0, 'y': 1}[dc_axis]
            dc_rail = stepper.LookupMultiRail(dc_config)
            dc_rail.setup_itersolve('cartesian_stepper_alloc', dc_axis.encode())
            for s in dc_rail.get_steppers():
                toolhead.register_step_generator(s.generate_steps)
            self.dual_carriage_rails = [
                self.rails[self.dual_carriage_axis], dc_rail]
            self.printer.lookup_object('gcode').register_command(
                'SET_DUAL_CARRIAGE', self.cmd_SET_DUAL_CARRIAGE,
                desc=self.cmd_SET_DUAL_CARRIAGE_help)
        self._respond_info = self.printer.lookup_object('gcode').respond_info
    def get_steppers(self):
        rails = self.rails
        if self.dual_carriage_axis is not None:
            dca = self.dual_carriage_axis
            rails = rails[:dca] + self.dual_carriage_rails + rails[dca+1:]
        return [s for rail in rails for s in rail.get_steppers()]
    def calc_position(self, stepper_positions):
        return [stepper_positions[rail.get_name()] for rail in self.rails]
    def set_position(self, newpos, homing_axes):
        for i, rail in enumerate(self.rails):
            rail.set_position(newpos)
            if i in homing_axes:
                self.limits[i] = rail.get_range()
    def note_z_not_homed(self):
        # Helper for Safe Z Home
        self.limits[2] = (1.0, -1.0)
    def _home_axis(self, homing_state, axis, rail):
        # Determine movement
        position_min, position_max = rail.get_range()
        hi = rail.get_homing_info()
        homepos = [None, None, None, None]
        homepos[axis] = hi.position_endstop
        forcepos = list(homepos)
        if hi.positive_dir:
            forcepos[axis] -= 1.5 * (hi.position_endstop - position_min)
        else:
            forcepos[axis] += 1.5 * (position_max - hi.position_endstop)
        self._respond_info(
            format_section(
                "CartKinematics._home_axis()",
                format_columns(
                    " | ",
                    format_key_value_list((
                        ("position_min", position_min),
                        ("position_max", position_max),
                        ("homing info", hi),
                        ("homepos", homepos),
                        ("forcepos", forcepos))),
                    format_stack())))
        # Perform homing
        homing_state.home_rails([rail], forcepos, homepos)
    def home(self, homing_state):
        # Each axis is homed independently and in order
        for axis in homing_state.get_axes():
            if axis == self.dual_carriage_axis:
                dc1, dc2 = self.dual_carriage_rails
                altc = self.rails[axis] == dc2
                self._activate_carriage(0)
                self._home_axis(homing_state, axis, dc1)
                self._activate_carriage(1)
                self._home_axis(homing_state, axis, dc2)
                self._activate_carriage(altc)
            else:
                self._home_axis(homing_state, axis, self.rails[axis])
    def _motor_off(self, print_time):
        self.limits = [(1.0, -1.0)] * 3
    def _check_endstops(self, move):
        end_pos = move.end_pos
        # self._respond_info(
        #     format_section(
        #         "CartKinematics._check_endstops()",
        #         format_columns(
        #             " # ",
        #             format_key_value_list((
        #                 ("end_pos", end_pos),
        #                 ("self.limits", self.limits))),
        #             format_stack(),
        #             str(move))))
        for i in (0, 1, 2):
            if (move.axes_d[i]
                and (end_pos[i] < self.limits[i][0]
                     or end_pos[i] > self.limits[i][1])):
                if self.limits[i][0] > self.limits[i][1]:
                    raise move.move_error("Must home axis first")
                raise move.move_error()
    def check_move(self, move):
        limits = self.limits
        xpos, ypos = move.end_pos[:2]
        # self._respond_info(
        #     format_section(
        #         "CartKinematics.check_move()",
        #         format_columns(
        #             " # ",
        #             format_key_value_list((
        #                 ("limits", limits),
        #                 ("xpos", xpos),
        #                 ("ypos", ypos),
        #                 ("x min", limits[0][0]),
        #                 ("x max", limits[0][1]),
        #                 ("y min", limits[1][0]),
        #                 ("y max", limits[1][1]))),
        #             format_stack(),
        #             str(move))))
        if (xpos < limits[0][0] or xpos > limits[0][1]
            or ypos < limits[1][0] or ypos > limits[1][1]):
            self._check_endstops(move)
        if not move.axes_d[2]:
            # Normal XY move - use defaults
            return
        # Move with Z - update velocity and accel for slower Z axis
        self._check_endstops(move)
        z_ratio = move.move_d / abs(move.axes_d[2])
        move.limit_speed(
            self.max_z_velocity * z_ratio, self.max_z_accel * z_ratio)
    def get_status(self, eventtime):
        axes = [a for a, (l, h) in zip("xyz", self.limits) if l <= h]
        return {
            'homed_axes': "".join(axes),
            'axis_minimum': self.axes_min,
            'axis_maximum': self.axes_max,
        }
    # Dual carriage support
    def _activate_carriage(self, carriage):
        toolhead = self.printer.lookup_object('toolhead')
        toolhead.flush_step_generation()
        dc_rail = self.dual_carriage_rails[carriage]
        dc_axis = self.dual_carriage_axis
        self.rails[dc_axis].set_trapq(None)
        dc_rail.set_trapq(toolhead.get_trapq())
        self.rails[dc_axis] = dc_rail
        pos = toolhead.get_position()
        pos[dc_axis] = dc_rail.get_commanded_position()
        toolhead.set_position(pos)
        if self.limits[dc_axis][0] <= self.limits[dc_axis][1]:
            self.limits[dc_axis] = dc_rail.get_range()
    cmd_SET_DUAL_CARRIAGE_help = "Set which carriage is active"
    def cmd_SET_DUAL_CARRIAGE(self, gcmd):
        carriage = gcmd.get_int('CARRIAGE', minval=0, maxval=1)
        self._activate_carriage(carriage)


def format_stack():
    return "".join(traceback.format_stack())


def format_value_change(old_value, new_value):
    return "%s -> %s" % (format_value(old_value), format_value(new_value))


def format_key_value_list(key_value_list):
    return format_columns(" : ",
                          "\n".join([str(key) for key, _ in key_value_list]),
                          "\n".join([format_value(value) for _, value in key_value_list]))


def format_section(title, content):
    return "%s:\n%s" % (title, indent(content))


def format_columns(col_separator="", *cols):
    max_col_lines = max([len(col.splitlines()) for col in cols])
    cols = list([col + "\n" * (max_col_lines - len(col.splitlines()) + 1) for col in cols])
    separator_cols = list(itertools.repeat((col_separator + "\n") * max_col_lines, len(cols) - 1))
    separated_cols = cols + separator_cols
    separated_cols[::2] = cols
    separated_cols[1::2] = separator_cols
    return append_lines_left_to_right(*[columnize(col, str.ljust) for col in separated_cols])


def columnize(string, just):
    max_line_length = max([len(line) for line in string.splitlines()])
    return "\n".join(just(line, max_line_length) for line in string.splitlines())


def append_lines_left_to_right(*texts):
    return "\n".join(["".join(lines)
                      for lines
                      in itertools.izip_longest(*[text.splitlines()
                                                  for text
                                                  in texts],
                                                fillvalue="")])


def indent(string):
    return "\n".join(["    %s" % line for line in string.splitlines()])


def format_value(value):
    format_functions = {
        int: lambda: "{:+.3f}".format(value),
        float: lambda: "{:+.3f}".format(value),
        list: lambda: "[%s]" % ", ".join(map(format_value, value)),
        tuple: lambda: "(%s)" % ", ".join(map(format_value, value)),
    }
    if format_functions.__contains__(type(value)):
        return format_functions[type(value)]()
    else:
        return str(value)


def load_kinematics(toolhead, config):
    return CartKinematics(toolhead, config)
