#pragma once

/* This file contains the display names for the commands in the UI's command
   palette. They are used both to key the associated command state object and to
   define the button text.

   We manually maintain a set of labels because the layout of the buttons in the
   UI is done by hand with some buttons having different sizes and positions.
   This code becomes clearer if it's easy to see what command we're working with
   (and not some entry in a dynamic container).  */

#define COMMAND_START "Start"
#define COMMAND_STOP "Stop"
#define COMMAND_SET_TOP "Set"
#define COMMAND_SET_CLOCK "Get"
#define COMMAND_FORCE "Force"
#define COMMAND_SET_CLOCK_SYSTEM "Set Clock\nSystem"
#define COMMAND_INITIALIZE "Initialize"
#define COMMAND_VALIDATE "Validate"
#define COMMAND_DEFAULT_ACQUISITION "32k\n15Hz"
#define COMMAND_SCALE_DOUBLE "x2"
#define COMMAND_SCALE_HALF "/2"
#define COMMAND_INTERNAL_REFERENCE "Internal\nClock Ref."
#define COMMAND_EXTERNAL_REFERENCE "External\nClock Ref."
#define COMMAND_EXTERNAL_CLOCK "External\nClock"
#define COMMAND_COPY_TOP "Copy Top\nFilename"
#define COMMAND_COPY_CLOCK "Copy Clock\nSystem\nFilename"
