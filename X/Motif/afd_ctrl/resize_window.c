/*
 *  resize_window.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2002 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   resize_window - resizes the window
 **
 ** SYNOPSIS
 **   signed char resize_window(void)
 **
 ** DESCRIPTION
 **   The size of the window is changed, and when the _AUTO_REPOSITION
 **   option is set, the window is repositioned when it touches the
 **   right or bottom end of the screen.
 **   The size of the label window is changed when the height in
 **   line (other font) has changed.
 **
 ** RETURN VALUES
 **   YES - Window has been resized
 **   NO  - Window has not been resized
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.01.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include <Xm/Xm.h>
#include "afd_ctrl.h"

extern Display      *display;
extern Widget       appshell,
                    line_window_w,
                    label_window_w,
                    button_window_w,
                    short_line_window_w;
extern int          line_height,
                    magic_value,
                    no_of_rows,
                    no_of_short_rows,
                    window_width,
                    window_height;
extern unsigned int glyph_height;


/*############################ resize_window() #########################*/
signed char
resize_window(void)
{
   static int old_long_window_height = -1,
              old_short_window_height = -1;
   int        long_window_height,
              ret,
              short_window_height;
   Arg        args[2];
   Cardinal   argcount;

   if (window_size(&window_width, &window_height) == YES)
   {
#ifdef _AUTO_REPOSITION
      XWindowAttributes window_attrib;
      int               display_width,
                        display_height,
                        new_x,
                        new_y;
      Position          root_x,
                        root_y;
#endif
      static int        old_line_height = 0;

#ifdef _AUTO_REPOSITION
      /* Get new window position */
      display_width = DisplayWidth(display, DefaultScreen(display));
      display_height = DisplayHeight(display, DefaultScreen(display));
      XGetWindowAttributes(display, XtWindow(appshell), &window_attrib);

      /* Translate coordinates relative to root window */
      XtTranslateCoords(appshell, window_attrib.x, window_attrib.y,
                        &root_x, &root_y);

      /* Change x coordinate */
      if ((root_x + window_width) > display_width)
      {
         new_x = display_width - window_width;

         /* Is window wider then display ? */
         if (new_x < 0)
         {
            new_x = 0;
         }
      }
      else
      {
         new_x = root_x;
      }

      /* Change y coordinate */
      if ((root_y + window_height + 23) > display_height)
      {
         new_y = display_height - window_height;

         /* Is window wider then display ? */
         if (new_y < 23)
         {
            new_y = 23;
         }
      }
      else
      {
         new_y = root_y;
      }

      /* Resize window */
      XMoveResizeWindow(display, XtWindow(appshell),
                        new_x, new_y,
                        window_width,
                        window_height + line_height + line_height + glyph_height + magic_value);
#else
      XResizeWindow(display, XtWindow(appshell), window_width,
                    window_height + line_height + line_height + glyph_height + magic_value);
#endif

      /* If the line_height changed, don't forget to change the */
      /* height of the label and button window!                 */
      argcount = 0;
      XtSetArg(args[argcount], XmNwidth, (Dimension)window_width);
      argcount++;
      long_window_height = no_of_rows * line_height;
      if (long_window_height != old_long_window_height)
      {
         XtSetArg(args[argcount], XmNheight, (Dimension)long_window_height);
         argcount++;
         old_long_window_height = long_window_height;
      }
      XtSetValues(line_window_w, args, argcount);
      argcount = 0;
      XtSetArg(args[argcount], XmNwidth, (Dimension)window_width);
      argcount++;
      short_window_height = no_of_short_rows * line_height;
      if (short_window_height != old_short_window_height)
      {
         XtSetArg(args[argcount], XmNheight, (Dimension)(short_window_height));
         argcount++;
         old_short_window_height = short_window_height;
      }
      XtSetValues(short_line_window_w, args, argcount);
      argcount = 0;
      XtSetArg(args[argcount], XmNwidth, (Dimension)window_width);
      argcount++;
      if (line_height != old_line_height)
      {
         XtSetArg(args[argcount], XmNheight, (Dimension)line_height);
         argcount++;
         old_line_height = line_height;
      }
      XtSetValues(label_window_w, args, argcount);
      XtSetValues(button_window_w, args, argcount);
      ret = YES;
   }
   else
   {
      long_window_height = no_of_rows * line_height;
      if (long_window_height != old_long_window_height)
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNheight, (Dimension)long_window_height);
         argcount++;
         XtSetValues(line_window_w, args, argcount);
         XtResizeWidget(line_window_w, window_width, long_window_height, 0);
      }
      short_window_height = (no_of_short_rows * line_height) + 1;
      if (short_window_height != old_short_window_height)
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNheight, (Dimension)(short_window_height));
         argcount++;
         XtSetValues(short_line_window_w, args, argcount);
         XtResizeWidget(short_line_window_w, window_width,
                        short_window_height, 0);
      }
      if ((old_long_window_height != -1) && (old_short_window_height != -1))
      {
         if (short_line_window_w->core.y != (line_window_w->core.y + long_window_height))
         {
            XtMoveWidget(short_line_window_w,
                         short_line_window_w->core.x,
                         line_window_w->core.y + long_window_height);
         }
      }
      old_long_window_height = long_window_height;
      old_short_window_height = short_window_height;
      ret = NO;
   }
   return(ret);
}
