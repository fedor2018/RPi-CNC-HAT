#!/usr/bin/env python
#
# Copyright (c) 2015 Serguei Glavatski ( verser  from cnc-club.ru )
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import hal                  # base hal class to react to hal signals
import os                   # needed to get the paths and directorys
import hal_glib             # needed to make our own hal pins
import gtk                  # base for pygtk widgets and constants
import gtk.glade
import sys                  # handle system calls
import linuxcnc             # to get our own error sytsem
import gobject              # needed to add the timer for periodic
import pygtk
import gladevcp
import pango
from linuxcnc import ini
import ConfigParser
from subprocess import Popen, PIPE


CONFIGPATH = os.environ['CONFIG_DIR']


cp = ConfigParser.RawConfigParser
class preferences(cp):
    types = {
        bool: cp.getboolean,
        float: cp.getfloat,
        int: cp.getint,
        str: cp.get,
        repr: lambda self, section, option: eval(cp.get(self, section, option)),
    }

    def __init__(self, path = None):
        cp.__init__(self)
        if not path:
            path = "~/.toolch_preferences"
        self.fn = os.path.expanduser(path)
        self.read(self.fn)

    def getpref(self, option, default = False, type = bool):
        m = self.types.get(type)
        try:
            o = m(self, "DEFAULT", option)
        except Exception, detail:
            print detail
            self.set("DEFAULT", option, default)
            self.write(open(self.fn, "w"))
            if type in(bool, float, int):
                o = type(default)
            else:
                o = default
        return o

    def putpref(self, option, value, type = bool):
        self.set("DEFAULT", option, type(value))
        self.write(open(self.fn, "w"))

# a class for holding the glade widgets rather then searching for them each time
class Widgets:

    def __init__(self, builder):
        self._builder = builder

    def __getattr__(self, attr):
        widget = self._builder.get_object(attr)
        if widget is None:
            raise AttributeError,  "No widget %widget" % attr
        return widget

    def __getitem__(self, attr):
        widget = self._builder.get_object(attr)
        if widget is None:
            raise IndexError, "No widget %widget" % attr
        return widget


class HandlerClass:

    # display warning dialog
    def warning_dialog(self, message, secondary = None, title = _("Operator Message")):
        dialog = gtk.MessageDialog(self.widgets.window1,
            gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_INFO, gtk.BUTTONS_OK, message)
        # if there is a secondary message then the first message text is bold
        if secondary:
            dialog.format_secondary_text(secondary)
        dialog.show_all()
        dialog.set_title(title)
        responce = dialog.run()
        dialog.destroy()
        return responce == gtk.RESPONSE_OK

        # Here we create a manual tool change dialog
    def on_tool_change( self, gtkbutton, data = None):
        change = self.halcomp['toolchange-change']
        toolnumber = self.halcomp['toolchange-number']
        print "toolnumber =",toolnumber,change
        if change:
            # if toolnumber = 0 we will get an error because we will not be able to get
            # any tooldescription, so we avoid that case
            if toolnumber == 0:
                message = _( "Please remove the mounted tool and press OK when done" )
            else:
                tooltable = self.inifile.find("EMCIO", "TOOL_TABLE")
                if not tooltable:
                    print( _( "**** auto_tool_measurement ERROR ****" ) )
                    print( _( "**** Did not find a toolfile file in [EMCIO] TOOL_TABLE ****" ) )
                    sys.exit()
                CONFIGPATH = os.environ['CONFIG_DIR']
                toolfile = os.path.join( CONFIGPATH, tooltable )
                self.tooledit1.set_filename( toolfile )
                tooldescr = self.tooledit1.get_toolinfo( toolnumber )[16]
                message = _( "Please change to tool\n\n# {0:d}     {1}\n\n then click OK." ).format( toolnumber, tooldescr )
            result = self.warning_dialog( message, title = _( "Manual Toolchange" ) )
            if result:
                self.halcomp["toolchange-changed"] = True
            else:
                print"toolchange abort", self.stat.tool_in_spindle, self.halcomp['toolchange-number']
                self.command.abort()
                self.halcomp['toolchange-number'] = self.stat.tool_in_spindle
                self.halcomp['toolchange-change'] = False
                self.halcomp['toolchange-changed'] = True
                self.messg = _( "Tool Change has been aborted!\n" )
                self.messg += _( "The old tool will remain set!" )
                self.warning_dialog( message)
        else:
            self.halcomp['toolchange-changed'] = False

    def get_preference_file_path(self):
        # we get the preference file, if there is none given in the INI
        # we use toolchange2.pref in the config dir
        temp = self.inifile.find("DISPLAY", "PREFERENCE_FILE_PATH")
        if not temp:
            machinename = self.inifile.find("EMC", "MACHINE")
            if not machinename:
                temp = os.path.join(CONFIGPATH, "manualtoolchange.pref")
            else:
                machinename = machinename.replace(" ", "_")
                temp = os.path.join(CONFIGPATH, "%s.pref" % machinename)
        print("**** auto_tool_measurement GETINIINFO **** \n Preference file path: %s" % temp)
        return temp

    def get_tool_sensor_data(self):
        xpos = self.inifile.find("TOOLSENSOR", "X")
        ypos = self.inifile.find("TOOLSENSOR", "Y")
        zpos = self.inifile.find("TOOLSENSOR", "Z")
        maxprobe = self.inifile.find("TOOLSENSOR", "MAXPROBE")
        return xpos, ypos, zpos, maxprobe

    def on_spbtn_probe_height_value_changed( self, gtkspinbutton, data = None ):
        self.halcomp["probeheight"] = gtkspinbutton.get_value()
        self.prefs.putpref( "probeheight", gtkspinbutton.get_value(), float )

    def on_spbtn_block_height_value_changed( self, gtkspinbutton, data = None ):
        blockheight = gtkspinbutton.get_value()
        if blockheight != False or blockheight == 0:
            self.halcomp["blockheight"] = blockheight
            self.halcomp["probeheight"] = self.spbtn_probe_height.get_value()
        else:
            self.prefs.putpref( "blockheight", 0.0, float )
            print( _( "Conversion error in btn_block_height" ) )
            self._add_alarm_entry( _( "Offset conversion error because off wrong entry" ) )
            self.warning_dialog( self, _( "Conversion error in btn_block_height!" ),
                                   _( "Please enter only numerical values\nValues have not been applied" ) )
        # set koordinate system to new origin
        origin = float(self.inifile.find("AXIS_2", "MIN_LIMIT")) + blockheight
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "G10 L2 P0 Z%s" % origin )
        self.widgets.hal_action_reload.emit( "activate" )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()

    def on_spbtn_search_vel_value_changed( self, gtkspinbutton, data = None ):
        self.halcomp["searchvel"] = gtkspinbutton.get_value()
        self.prefs.putpref( "searchvel", gtkspinbutton.get_value(), float )

    def on_spbtn_probe_vel_value_changed( self, gtkspinbutton, data = None ):
        self.halcomp["probevel"] = gtkspinbutton.get_value()
        self.prefs.putpref( "probevel", gtkspinbutton.get_value(), float )

    def on_spbtn_touch_off_x_value_changed( self, gtkspinbutton, data = None ):
        self.prefs.putpref( "touch_off_x", gtkspinbutton.get_value(), float )

    def on_spbtn_touch_off_y_value_changed( self, gtkspinbutton, data = None ):
        self.prefs.putpref( "touch_off_y", gtkspinbutton.get_value(), float )

    def on_spbtn_touch_off_z_value_changed( self, gtkspinbutton, data = None ):
        self.prefs.putpref( "touch_off_z", gtkspinbutton.get_value(), float )

    def clicked_btn_probe_tool_setter(self, data = None):
        # Start probe_down.ngc
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "O<probe_down> call" )
        self.stat.poll()
        while self.stat.exec_state == 7 or self.stat.exec_state == 3 :
            self.command.wait_complete()
            self.stat.poll()
        self.command.wait_complete()
        a=self.stat.probed_position
        self.spbtn_probe_height.set_value( float(a[2]) )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()

    def clicked_btn_probe_workpiece(self, data = None):
        # Start probe_down.ngc
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "O<block_down> call" )
        self.stat.poll()
        while self.stat.exec_state == 7 or self.stat.exec_state == 3 :
            self.command.wait_complete()
            self.stat.poll()
        self.command.wait_complete()
        a=self.stat.probed_position
        self.spbtn_block_height.set_value( float(a[2]) )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()

    def on_spbtn_tool_number_value_changed( self, gtkspinbutton, data = None ):
        self.halcomp["toolchange-number"] = gtkspinbutton.get_value()
        self.prefs.putpref( "toolchange-number", gtkspinbutton.get_value(), float )

    def clicked_btn_manual_change( self, data = None ):
        # Start probe_down.ngc
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "M6 T%f" % self.spbtn_tool_number.get_value())
        self.command.wait_complete()
        self.command.mdi( "G43 H%f" % self.spbtn_tool_number.get_value())
        self.command.wait_complete()
        self.widgets.hal_action_reload.emit( "activate" )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()


    def clicked_btn_set_x( self, data = None ):
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "G10 L20 P0 X%f" % self.spbtn_touch_off_x.get_value() )
        self.command.wait_complete()
        self.widgets.hal_action_reload.emit( "activate" )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()
        self.prefs.putpref( "touch_off_x", self.spbtn_touch_off_x.get_value(), float )

    def clicked_btn_set_y( self, data = None ):
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "G10 L20 P0 Y%f" % self.spbtn_touch_off_y.get_value() )
        self.command.wait_complete()
        self.widgets.hal_action_reload.emit( "activate" )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()
        self.prefs.putpref( "touch_off_y", self.spbtn_touch_off_y.get_value(), float )

    def clicked_btn_set_z( self, data = None ):
        self.command.mode( linuxcnc.MODE_MDI )
        self.command.wait_complete()
        self.command.mdi( "G10 L20 P0 Z%f" % self.spbtn_touch_off_z.get_value() )
        self.command.wait_complete()
        self.widgets.hal_action_reload.emit( "activate" )
        self.command.mode( linuxcnc.MODE_MANUAL )
        self.command.wait_complete()
        self.prefs.putpref( "touch_off_z", self.spbtn_touch_off_z.get_value(), float )

#    def _init_preferences( self ):

    def on_chk_use_tool_measurement_toggled( self, gtkcheckbutton, data = None ):
        if gtkcheckbutton.get_active():
            self.frm_probe_pos.set_sensitive( True )
            self.frm_probe_vel.set_sensitive( True )
            self.frm_block.set_sensitive( True )
            self.frm_offsets.set_sensitive( True )
            self.frm_misc.set_sensitive( True )
            self.halcomp["toolmeasurement"] = True
            self.halcomp["searchvel"] = self.spbtn_search_vel.get_value()
            self.halcomp["probevel"] = self.spbtn_probe_vel.get_value()
            self.halcomp["probeheight"] = self.spbtn_probe_height.get_value()
            self.halcomp["blockheight"] = self.spbtn_block_height.get_value()
        else:
            self.frm_probe_pos.set_sensitive( False )
            self.frm_probe_vel.set_sensitive( False )
            self.frm_block.set_sensitive( False )
            self.frm_offsets.set_sensitive( False )
            self.frm_misc.set_sensitive( False )
            self.halcomp["toolmeasurement"] = False
            self.halcomp["searchvel"] = 0.0
            self.halcomp["probevel"] = 0.0
            self.halcomp["probeheight"] = 0.0
            self.halcomp["blockheight"] = 0.0
        self.prefs.putpref( "use_toolmeasurement", gtkcheckbutton.get_active(), bool )

    def __init__(self, halcomp,builder,useropts):
        inipath = os.environ["INI_FILE_NAME"]
        self.inifile = ini(inipath)
        if not self.inifile:
            print("**** auto_tool_measurement GETINIINFO **** \n Error, no INI File given !!")
            sys.exit()
        self.command = linuxcnc.command()
        self.stat = linuxcnc.stat()
        self.builder = builder
        self.widgets = Widgets( self.builder )
        # set the title of the window
        self.frm_probe_pos = self.builder.get_object("frm_probe_pos")
        self.frm_probe_vel = self.builder.get_object("frm_probe_vel")
        self.frm_block = self.builder.get_object("frm_block")
        self.frm_offsets = self.builder.get_object("frm_offsets")
        self.frm_misc = self.builder.get_object("frm_misc")
        self.spbtn_search_vel = self.builder.get_object("spbtn_search_vel")
        self.spbtn_probe_vel = self.builder.get_object("spbtn_probe_vel")
        self.spbtn_probe_height = self.builder.get_object("spbtn_probe_height")
        self.chk_use_tool_measurement = self.builder.get_object("chk_use_tool_measurement")
        self.lbl_x_probe = self.builder.get_object("lbl_x_probe")
        self.lbl_y_probe = self.builder.get_object("lbl_y_probe")
        self.lbl_z_probe = self.builder.get_object("lbl_z_probe")
        self.lbl_maxprobe = self.builder.get_object("lbl_maxprobe")
        self.spbtn_block_height = self.builder.get_object("spbtn_block_height")
        self.btn_probe_tool_setter = self.builder.get_object("btn_probe_tool_setter")
        self.btn_probe_workpiece = self.builder.get_object("btn_probe_workpiece")
        self.btn_set_x = self.builder.get_object("btn_set_x")
        self.btn_set_y = self.builder.get_object("btn_set_y")
        self.btn_set_z = self.builder.get_object("btn_set_z")
        self.spbtn_touch_off_x = self.builder.get_object("spbtn_touch_off_x")
        self.spbtn_touch_off_y = self.builder.get_object("spbtn_touch_off_y")
        self.spbtn_touch_off_z = self.builder.get_object("spbtn_touch_off_z")
        self.spbtn_tool_number = self.builder.get_object("spbtn_tool_number")
        self.btn_manual_change = self.builder.get_object("btn_manual_change")
        self.tooledit1 = self.builder.get_object("tooledit1")
        self.messg = " "

        self.prefs = preferences( self.get_preference_file_path() )
        self.halcomp = hal.component("auto_tool_measurement")
        self.change_text = builder.get_object("change-text")
        self.halcomp.newpin("number", hal.HAL_FLOAT, hal.HAL_IN)
        # make the pins for tool measurement
        self.halcomp.newpin( "probeheight", hal.HAL_FLOAT, hal.HAL_OUT )
        self.halcomp.newpin( "blockheight", hal.HAL_FLOAT, hal.HAL_OUT )
        self.halcomp.newpin( "toolmeasurement", hal.HAL_BIT, hal.HAL_OUT )
        self.halcomp.newpin( "searchvel", hal.HAL_FLOAT, hal.HAL_OUT )
        self.halcomp.newpin( "probevel", hal.HAL_FLOAT, hal.HAL_OUT )
        # for manual tool change dialog
        self.halcomp.newpin( "toolchange-number", hal.HAL_S32, hal.HAL_IN )
        self.halcomp.newpin( "toolchange-changed", hal.HAL_BIT, hal.HAL_OUT )
        pin = self.halcomp.newpin( 'toolchange-change', hal.HAL_BIT, hal.HAL_IN )
        hal_glib.GPin( pin ).connect( 'value_changed', self.on_tool_change )
        self.halcomp['toolchange-number'] = self.stat.tool_in_spindle
        # tool measurement probe settings
        xpos, ypos, zpos, maxprobe = self.get_tool_sensor_data()
        if not xpos or not ypos or not zpos or not maxprobe:
            self.chk_use_tool_measurement.set_active( False )
            self.chk_use_tool_measurement.set_sensitive( False )
            print( _( "**** auto_tool_measurement INFO ****" ) )
            print( _( "**** no valid probe config in INI File ****" ) )
            print( _( "**** disabled tool measurement ****" ) )
        else:
            self.spbtn_probe_height.set_value( self.prefs.getpref( "probeheight", -1.0, float ) )
            self.spbtn_block_height.set_value( self.prefs.getpref( "blockheight", -1.0, float ) )
            self.spbtn_search_vel.set_value( self.prefs.getpref( "searchvel", 75.0, float ) )
            self.spbtn_probe_vel.set_value( self.prefs.getpref( "probevel", 10.0, float ) )

            self.spbtn_touch_off_x.set_value( self.prefs.getpref( "touch_off_x", 0, float ) )
            self.spbtn_touch_off_y.set_value( self.prefs.getpref( "touch_off_y", 0, float ) )
            self.spbtn_touch_off_z.set_value( self.prefs.getpref( "touch_off_z", 0, float ) )

            self.lbl_x_probe.set_label( str( xpos ) )
            self.lbl_y_probe.set_label( str( ypos ) )
            self.lbl_z_probe.set_label( str( zpos ) )
            self.lbl_maxprobe.set_label( str( maxprobe ) )
            self.chk_use_tool_measurement.set_active( self.prefs.getpref( "use_toolmeasurement", False, bool ) )
            # to set the hal pin with correct values we emit a toogled
#            self.chk_use_tool_measurement.emit( "toggled" )
            if self.chk_use_tool_measurement.get_active():
                self.frm_probe_pos.set_sensitive( True )
                self.frm_probe_vel.set_sensitive( True )
                self.frm_block.set_sensitive( True )
                self.frm_offsets.set_sensitive( True )
                self.frm_misc.set_sensitive( True )
                self.halcomp["toolmeasurement"] = True
                self.halcomp["searchvel"] = self.spbtn_search_vel.get_value()
                self.halcomp["probevel"] = self.spbtn_probe_vel.get_value()
                self.halcomp["probeheight"] = self.spbtn_probe_height.get_value()
                self.halcomp["blockheight"] = self.spbtn_block_height.get_value()
            else:
                self.frm_probe_pos.set_sensitive( False )
                self.frm_probe_vel.set_sensitive( False )
                self.frm_block.set_sensitive( False )
                self.frm_offsets.set_sensitive( False )
                self.frm_misc.set_sensitive( False )




def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
