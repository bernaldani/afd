/*
 *  draw_line.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   draw_line - draws one complete line of the afd_ctrl window
 **
 ** SYNOPSIS
 **   void draw_label_line(void)
 **   void draw_line_status(int pos, signed char delta)
 **   void draw_button_line(void)
 **   void draw_blank_line(int pos)
 **   void draw_long_blank_line(int pos)
 **   void draw_dest_identifier(Window w, int pos, int x, int y)
 **   void draw_debug_led(int pos, int x, int y)
 **   void draw_led(int pos, int led_no, int x, int y)
 **   void draw_proc_led(int led_no, signed char led_status)
 **   void draw_log_status(int log_typ, int si_pos)
 **   void draw_queue_counter(int queue_counter)
 **   void draw_proc_stat(int pos, int job_no, int x, int y)
 **   void draw_detailed_selection(int pos, int job_no)
 **   void draw_chars(int pos, char type, int x, int y, int column)
 **   void draw_bar(int pos, signed cahr delta, char bar_no, int x, int y, int column);
 **
 ** DESCRIPTION
 **   The function draw_label_line() draws the label which is just
 **   under the menu bar. It draws the following labels: host, fc,
 **   fs, tr and ec when character style is set.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.01.1996 H.Kiehl Created
 **   30.08.1997 H.Kiehl Removed all sprintf().
 **   03.09.1997 H.Kiehl Added AFDD Led.
 **   22.12.2001 H.Kiehl Added variable column length.
 **   26.12.2001 H.Kiehl Allow for more changes in line style.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "afd_ctrl.h"

extern Display                    *display;
extern Widget                     appshell;
extern Window                     label_window,
                                  line_window,
                                  button_window,
                                  short_line_window;
extern GC                         letter_gc,
                                  normal_letter_gc,
                                  locked_letter_gc,
                                  color_letter_gc,
                                  default_bg_gc,
                                  normal_bg_gc,
                                  locked_bg_gc,
                                  label_bg_gc,
                                  button_bg_gc,
                                  tr_bar_gc,
                                  color_gc,
                                  black_line_gc,
                                  white_line_gc,
                                  led_gc;
extern Colormap                   default_cmap;
extern char                       line_style;
extern unsigned long              color_pool[];
extern float                      max_bar_length;
extern int                        *line_length,
                                  max_line_length,
                                  line_height,
                                  bar_thickness_2,
                                  button_width,
                                  led_width,
                                  no_of_long_lines,
                                  no_of_short_columns,
                                  short_line_length,
                                  window_width,
                                  x_offset_debug_led,
                                  x_offset_led,
                                  x_offset_proc,
                                  x_offset_bars,
                                  x_offset_characters,
                                  x_offset_stat_leds,
                                  x_offset_receive_log,
                                  x_center_receive_log,
                                  x_offset_sys_log,
                                  x_center_sys_log,
                                  x_offset_trans_log,
                                  x_center_trans_log,
                                  y_offset_led,
                                  y_center_log,
                                  log_angle,
                                  no_of_columns;
extern unsigned int               glyph_height,
                                  glyph_width,
                                  text_offset;
extern struct coord               coord[3][LOG_FIFO_SIZE];
extern struct afd_status          prev_afd_status;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;

#ifdef _DEBUG
static unsigned int  counter = 0;
#endif


/*########################## draw_label_line() ##########################*/
void
draw_label_line(void)
{
   int i,
       x = 0;

   if (no_of_long_lines > 0)
   {
      for (i = 0; i < no_of_columns; i++)
      {
         /* First draw the background in the appropriate color */
         XFillRectangle(display, label_window, label_bg_gc,
                        x + 2,
                        2,
                        x + line_length[i] - 2,
                        line_height - 4);

         /* Now draw left, top and bottom end for button style */
         XDrawLine(display, label_window, black_line_gc,
                   x,
                   0,
                   x,
                   line_height);
         XDrawLine(display, label_window, white_line_gc,
                   x + 1,
                   1,
                   x + 1,
                   line_height - 3);
         XDrawLine(display, label_window, black_line_gc,
                   x,
                   0,
                   x + line_length[i],
                   0);
         XDrawLine(display, label_window, white_line_gc,
                   x + 1,
                   1,
                   x + line_length[i],
                   1);
         XDrawLine(display, label_window, black_line_gc,
                   x,
                   line_height - 2,
                   x + line_length[i],
                   line_height - 2);
         XDrawLine(display, label_window, white_line_gc,
                   x,
                   line_height - 1,
                   x + line_length[i],
                   line_height - 1);

         /* Draw string "  host" */
         XDrawString(display, label_window, letter_gc,
                     x + DEFAULT_FRAME_SPACE,
                     text_offset + SPACE_ABOVE_LINE,
                     "  host",
                     6);

         /* See if we need to extend heading for "Character" display */
         if (line_style & SHOW_CHARACTERS)
         {
            /* Draw string " fc   fs   tr  ec" */
            XDrawString(display, label_window, letter_gc,
                        x + x_offset_characters - (max_line_length - line_length[i]),
                        text_offset + SPACE_ABOVE_LINE,
                        " fc   fs   tr  ec",
                        17);
         }
         x += line_length[i];
      }
   }
   else
   {
      for (i = 0; i < no_of_short_columns; i++)
      {
         /* First draw the background in the appropriate color */
         XFillRectangle(display, label_window, label_bg_gc,
                        x + 2,
                        2,
                        x + short_line_length - 2,
                        line_height - 4);

         /* Now draw left, top and bottom end for button style */
         XDrawLine(display, label_window, black_line_gc,
                   x,
                   0,
                   x,
                   line_height);
         XDrawLine(display, label_window, white_line_gc,
                   x + 1,
                   1,
                   x + 1,
                   line_height - 3);
         XDrawLine(display, label_window, black_line_gc,
                   x,
                   0,
                   x + short_line_length,
                   0);
         XDrawLine(display, label_window, white_line_gc,
                   x + 1,
                   1,
                   x + short_line_length,
                   1);
         XDrawLine(display, label_window, black_line_gc,
                   x,
                   line_height - 2,
                   x + short_line_length,
                   line_height - 2);
         XDrawLine(display, label_window, white_line_gc,
                   x,
                   line_height - 1,
                   x + short_line_length,
                   line_height - 1);

         /* Draw string "  host" */
         XDrawString(display, label_window, letter_gc,
                     x + DEFAULT_FRAME_SPACE,
                     text_offset + SPACE_ABOVE_LINE,
                     "  host",
                     6);
         x += short_line_length;
      }
   }

   /* Draw right end for button style */
   XDrawLine(display, label_window, black_line_gc,
             x - 2,
             0,
             x - 2,
             line_height - 2);
   XDrawLine(display, label_window, white_line_gc,
             x - 1,
             1,
             x - 1,
             line_height - 2);

   return;
}


/*######################### draw_line_status() ##########################*/
void
draw_line_status(int pos, signed char delta)
{
   if (connect_data[pos].long_pos > -1)
   {
      int column, i, x, y;

      /* First locate position of x and y */
      locate_xy_column(connect_data[pos].long_pos, &x, &y, &column);

#ifdef _DEBUG
      (void)printf("Drawing long line %d %d  x = %d  y = %d\n",
                   pos, counter++, x, y);
#endif

      if ((connect_data[pos].inverse > OFF) && (delta >= 0))
      {
         if (connect_data[pos].inverse == ON)
         {
            XFillRectangle(display, line_window, normal_bg_gc, x, y,
                           line_length[column], line_height);
         }
         else
         {
            XFillRectangle(display, line_window, locked_bg_gc, x, y,
                           line_length[column], line_height);
         }
      }
      else
      {
         XFillRectangle(display, line_window, default_bg_gc, x, y,
                        line_length[column], line_height);
      }

      /* Write destination identifier to screen */
      draw_dest_identifier(line_window, pos, x, y);

      if (line_style & SHOW_LEDS)
      {
         /* Draw debug led */
         draw_debug_led(pos, x, y);

         /* Draw status LED's */
         draw_led(pos, 0, x, y);
         draw_led(pos, 1, x + led_width + LED_SPACING, y);
      }

      if (line_style & SHOW_JOBS)
      {
         /* Draw status button for each parallel transfer */
         for (i = 0; i < fsa[pos].allowed_transfers; i++)
         {
            draw_proc_stat(pos, i, x, y);
         }
      }

      /* Print information for number of files to be send (nf), */
      /* total file size (tfs), transfer rate (tr) and error    */
      /* counter (ec).                                          */
      if (line_style & SHOW_CHARACTERS)
      {
         draw_chars(pos, NO_OF_FILES, x, y, column);
         draw_chars(pos, TOTAL_FILE_SIZE, x + (5 * glyph_width), y, column);
         draw_chars(pos, TRANSFER_RATE, x + (10 * glyph_width), y, column);
         draw_chars(pos, ERROR_COUNTER, x + (15 * glyph_width), y, column);
      }

      /* Draw bars, indicating graphically how many errors have */
      /* occurred, total file size still to do and the transfer */
      /* rate.                                                  */
      if (line_style & SHOW_BARS)
      {
         /* Draw bars */
         draw_bar(pos, delta, TR_BAR_NO, x, y, column);
         draw_bar(pos, delta, ERROR_BAR_NO, x, y + bar_thickness_2, column);

         /* Show beginning and end of bars */
         if (connect_data[pos].inverse > OFF)
         {
            XDrawLine(display, line_window, white_line_gc,
                      x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                      y + SPACE_ABOVE_LINE,
                      x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                      y + glyph_height);
            XDrawLine(display, line_window, white_line_gc,
                      x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length,
                      y + SPACE_ABOVE_LINE,
                      x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length, y + glyph_height);
         }
         else
         {
            XDrawLine(display, line_window, black_line_gc,
                      x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                      y + SPACE_ABOVE_LINE,
                      x + x_offset_bars - (max_line_length - line_length[column]) - 1,
                      y + glyph_height);
            XDrawLine(display, line_window, black_line_gc,
                      x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length,
                      y + SPACE_ABOVE_LINE,
                      x + x_offset_bars - (max_line_length - line_length[column]) + (int)max_bar_length, y + glyph_height);
         }
      }
   }
   else
   {
      int x, y;

      /* First locate position of x and y */
      locate_xy_short(connect_data[pos].short_pos, &x, &y);

#ifdef _DEBUG
      (void)printf("Drawing short line %d %d  x = %d  y = %d\n",
                   pos, counter++, x, y);
#endif

      if ((connect_data[pos].inverse > OFF) && (delta >= 0))
      {
         if (connect_data[pos].inverse == ON)
         {
            XFillRectangle(display, short_line_window, normal_bg_gc, x, y,
                           short_line_length, line_height);
         }
         else
         {
            XFillRectangle(display, short_line_window, locked_bg_gc, x, y,
                           short_line_length, line_height);
         }
      }
      else
      {
         XFillRectangle(display, short_line_window, default_bg_gc, x, y,
                        short_line_length, line_height);
      }

      /* Write destination identifier to screen */
      draw_dest_identifier(short_line_window, pos, x, y);
   }

   return;
}


/*########################## draw_button_line() #########################*/
void
draw_button_line(void)
{
   XFillRectangle(display, button_window, button_bg_gc, 0, 0, window_width, line_height + 1);

   /* */
/*
   XDrawLine(display, button_window, black_line_gc, 0, 0, window_width, 0);
*/

   /* Draw status LED's for AFD */
   draw_proc_led(AMG_LED, prev_afd_status.amg);
   draw_proc_led(FD_LED, prev_afd_status.fd);
   draw_proc_led(AW_LED, prev_afd_status.archive_watch);
   if (prev_afd_status.afdd != NEITHER)
   {
      draw_proc_led(AFDD_LED, prev_afd_status.afdd);
   }

   /* Draw log status indicators */
   draw_log_status(RECEIVE_LOG_INDICATOR,
                   prev_afd_status.receive_log_ec % LOG_FIFO_SIZE);
   draw_log_status(SYS_LOG_INDICATOR,
                   prev_afd_status.sys_log_ec % LOG_FIFO_SIZE);
   draw_log_status(TRANS_LOG_INDICATOR,
                   prev_afd_status.trans_log_ec % LOG_FIFO_SIZE);

   /* Draw job queue counter */
   draw_queue_counter(prev_afd_status.jobs_in_queue);

   return;
}


/*########################## draw_blank_line() ##########################*/
void
draw_blank_line(int pos)
{
   int x, y;

   if (connect_data[pos].long_pos > -1)
   {
      int column;

      locate_xy_column(connect_data[pos].long_pos, &x, &y, &column);
      XFillRectangle(display, line_window, default_bg_gc, x, y,
                     line_length[column], line_height);
   }
   else
   {
      locate_xy_short(connect_data[pos].short_pos, &x, &y);
      XFillRectangle(display, short_line_window, default_bg_gc, x, y,
                     short_line_length, line_height);
   }

   return;
}


/*####################### draw_long_blank_line() ########################*/
void
draw_long_blank_line(int pos)
{
   int column,
       long_pos,
       x, y;

   if (pos >= no_of_long_lines)
   {
      long_pos = pos;
   }
   else
   {
      long_pos = connect_data[pos].long_pos;
   }
   locate_xy_column(long_pos, &x, &y, &column);
   XFillRectangle(display, line_window, default_bg_gc, x, y,
                  line_length[column], line_height);

   return;
}


/*+++++++++++++++++++++++ draw_dest_identifier() ++++++++++++++++++++++++*/
void
draw_dest_identifier(Window w, int pos, int x, int y)
{
   XGCValues gc_values;

   if (connect_data[pos].special_flag & HOST_IN_DIR_CONFIG)
   {
      /* Change color of letters when background color is to dark */
      if ((connect_data[pos].stat_color_no == TRANSFER_ACTIVE) ||
          (connect_data[pos].stat_color_no == NOT_WORKING2) ||
          (connect_data[pos].stat_color_no == PAUSE_QUEUE) ||
          ((connect_data[pos].stat_color_no == STOP_TRANSFER) &&
          (fsa[pos].active_transfers > 0)))
      {
         gc_values.foreground = color_pool[WHITE];
      }
      else
      {
         gc_values.foreground = color_pool[FG];
      }
      gc_values.background = color_pool[connect_data[pos].stat_color_no];
   }
   else
   {
      /* The host is NOT in the DIR_CONFIG, show default background. */
      if (connect_data[pos].inverse == OFF)
      {
         gc_values.background = color_pool[DEFAULT_BG];
         gc_values.foreground = color_pool[FG];
      }
      else if (connect_data[pos].inverse == ON)
           {
              gc_values.background = color_pool[BLACK];
              gc_values.foreground = color_pool[WHITE];
           }
           else
           {
              gc_values.background = color_pool[LOCKED_INVERSE];
              gc_values.foreground = color_pool[WHITE];
           }
   }
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);

   XDrawImageString(display, w, color_letter_gc,
                    DEFAULT_FRAME_SPACE + x,
                    y + text_offset + SPACE_ABOVE_LINE,
                    connect_data[pos].host_display_str,
                    MAX_HOSTNAME_LENGTH);

   return;
}


/*+++++++++++++++++++++++++ draw_debug_led() ++++++++++++++++++++++++++++*/
void
draw_debug_led(int pos, int x, int y)
{
   int       x_offset,
             y_offset;
   XGCValues gc_values;

   x_offset = x + x_offset_debug_led;
   y_offset = y + SPACE_ABOVE_LINE + y_offset_led;

   if (connect_data[pos].debug == YES)
   {
      gc_values.foreground = color_pool[DEBUG_MODE];
   }
   else
   {
      if (connect_data[pos].inverse == OFF)
      {
         gc_values.foreground = color_pool[DEFAULT_BG];
      }
      else if (connect_data[pos].inverse == ON)
           {
              gc_values.foreground = color_pool[BLACK];
           }
           else
           {
              gc_values.foreground = color_pool[LOCKED_INVERSE];
           }
   }
   XChangeGC(display, color_gc, GCForeground, &gc_values);
#ifdef _SQUARE_LED
   XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                  glyph_width, glyph_width);
#else
   XFillArc(display, line_window, color_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);
#endif

   if (connect_data[pos].inverse == OFF)
   {
#ifdef _SQUARE_LED
      XDrawRectangle(display, line_window, black_line_gc, x_offset, y_offset,
                     glyph_width, glyph_width);
#else
      XDrawArc(display, line_window, black_line_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
#endif
   }
   else
   {
#ifdef _SQUARE_LED
      XDrawRectangle(display, line_window, white_line_gc, x_offset, y_offset,
                     glyph_width, glyph_width);
#else
      XDrawArc(display, line_window, white_line_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
#endif
   }

   return;
}


/*++++++++++++++++++++++++++++ draw_led() ++++++++++++++++++++++++++++++*/
void
draw_led(int pos, int led_no, int x, int y)
{
   int       x_offset,
             y_offset;
   XGCValues gc_values;

   x_offset = x + x_offset_led;
   y_offset = y + SPACE_ABOVE_LINE;

   gc_values.foreground = color_pool[(int)connect_data[pos].status_led[led_no]];
   XChangeGC(display, color_gc, GCForeground, &gc_values);
   XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                  led_width, glyph_height);
#ifndef _NO_LED_FRAME
   if (connect_data[pos].inverse == OFF)
   {
      XDrawRectangle(display, line_window, black_line_gc, x_offset, y_offset,
                     led_width, glyph_height);
   }
   else
   {
      XDrawRectangle(display, line_window, white_line_gc, x_offset, y_offset,
                     led_width, glyph_height);
   }
#endif /* _NO_LED_FRAME */

   return;
}


/*++++++++++++++++++++++++++ draw_proc_led() ++++++++++++++++++++++++++++*/
void
draw_proc_led(int led_no, signed char led_status)
{
   int       x_offset,
             y_offset;
   GC        tmp_gc;
   XGCValues gc_values;

   x_offset = x_offset_stat_leds + (led_no * (glyph_width + PROC_LED_SPACING));
   y_offset = SPACE_ABOVE_LINE + y_offset_led;

   if (led_status == ON)
   {
      XFillArc(display, button_window, led_gc, x_offset, y_offset,
               glyph_width, glyph_width, 0, 23040);
      tmp_gc = black_line_gc;
   }
   else
   {
      if (led_status == OFF)
      {
         gc_values.foreground = color_pool[NOT_WORKING2];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillArc(display, button_window, color_gc, x_offset, y_offset,
                  glyph_width, glyph_width, 0, 23040);
         tmp_gc = black_line_gc;
      }
      else if (led_status == STOPPED)
           {
              gc_values.foreground = color_pool[STOP_TRANSFER];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              tmp_gc = black_line_gc;
           }
      else if (led_status == SHUTDOWN)
           {
              gc_values.foreground = color_pool[CLOSING_CONNECTION];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              tmp_gc = black_line_gc;
           }
      else if (led_status == NEITHER)
           {
              XFillArc(display, button_window, button_bg_gc, x_offset,
                       y_offset, glyph_width, glyph_width, 0, 23040);
              tmp_gc = button_bg_gc;
           }
           else
           {
              gc_values.foreground = color_pool[(int)led_status];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc, x_offset, y_offset,
                       glyph_width, glyph_width, 0, 23040);
              tmp_gc = black_line_gc;
           }
   }

   /* Draw LED frame. */
   XDrawArc(display, button_window, tmp_gc, x_offset, y_offset,
            glyph_width, glyph_width, 0, 23040);

   return;
}


/*++++++++++++++++++++++++ draw_log_status() ++++++++++++++++++++++++++++*/
void
draw_log_status(int log_typ, int si_pos)
{
   int       i,
             prev_si_pos;
   XGCValues gc_values;

   if (si_pos == 0)
   {
      prev_si_pos = LOG_FIFO_SIZE - 1;
   }
   else
   {
      prev_si_pos = si_pos - 1;
   }
   if (log_typ == SYS_LOG_INDICATOR)
   {
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         gc_values.foreground = color_pool[(int)prev_afd_status.sys_log_fifo[i]];
         XChangeGC(display, color_gc, GCForeground, &gc_values);
         XFillArc(display, button_window, color_gc,
                  x_offset_sys_log, SPACE_ABOVE_LINE,
                  glyph_height, glyph_height,
                  ((i * log_angle) * 64),
                  (log_angle * 64));
      }
      if ((prev_afd_status.sys_log_fifo[si_pos] == BLACK) ||
          (prev_afd_status.sys_log_fifo[prev_si_pos] == BLACK))
      {
         XDrawLine(display, button_window, white_line_gc,
                   x_center_sys_log, y_center_log,
                   coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
      }
      else
      {
         XDrawLine(display, button_window, black_line_gc,
                   x_center_sys_log, y_center_log,
                   coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
      }
   }
   else if (log_typ == TRANS_LOG_INDICATOR)
        {
           for (i = 0; i < LOG_FIFO_SIZE; i++)
           {
              gc_values.foreground = color_pool[(int)prev_afd_status.trans_log_fifo[i]];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc,
                       x_offset_trans_log, SPACE_ABOVE_LINE,
                       glyph_height, glyph_height,
                       ((i * log_angle) * 64),
                       (log_angle * 64));
           }
           if ((prev_afd_status.trans_log_fifo[si_pos] == BLACK) ||
               (prev_afd_status.trans_log_fifo[prev_si_pos] == BLACK))
           {
              XDrawLine(display, button_window, white_line_gc,
                        x_center_trans_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
           else
           {
              XDrawLine(display, button_window, black_line_gc,
                        x_center_trans_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
        }
        else
        {
           for (i = 0; i < LOG_FIFO_SIZE; i++)
           {
              gc_values.foreground = color_pool[(int)prev_afd_status.receive_log_fifo[i]];
              XChangeGC(display, color_gc, GCForeground, &gc_values);
              XFillArc(display, button_window, color_gc,
                       x_offset_receive_log, SPACE_ABOVE_LINE,
                       glyph_height, glyph_height,
                       ((i * log_angle) * 64),
                       (log_angle * 64));
           }
           if ((prev_afd_status.receive_log_fifo[si_pos] == BLACK) ||
               (prev_afd_status.receive_log_fifo[prev_si_pos] == BLACK))
           {
              XDrawLine(display, button_window, white_line_gc,
                        x_center_receive_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
           else
           {
              XDrawLine(display, button_window, black_line_gc,
                        x_center_receive_log, y_center_log,
                        coord[log_typ][si_pos].x, coord[log_typ][si_pos].y);
           }
        }

   return;
}


/*+++++++++++++++++++++++ draw_queue_counter() ++++++++++++++++++++++++++*/
void
draw_queue_counter(int queue_counter)
{
   char      string[5];
   XGCValues gc_values;

   if ((queue_counter > DANGER_NO_OF_JOBS) &&
#ifdef _LINK_MAX_TEST
       (queue_counter <= (LINKY_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
#else
#ifdef REDUCED_LINK_MAX
       (queue_counter <= (REDUCED_LINK_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
#else
       (queue_counter <= (LINK_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
#endif
#endif
   {
      gc_values.background = color_pool[WARNING_ID];
      gc_values.foreground = color_pool[FG];
   }
#ifdef _LINK_MAX_TEST
   else if (queue_counter > (LINKY_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR))
#else
#ifdef REDUCED_LINK_MAX
   else if (queue_counter > (REDUCED_LINK_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR))
#else
   else if (queue_counter > (LINK_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR))
#endif
#endif
        {
           gc_values.background = color_pool[ERROR_ID];
           gc_values.foreground = color_pool[WHITE];
        }
        else
        {
           gc_values.background = color_pool[CHAR_BACKGROUND];
           gc_values.foreground = color_pool[FG];
        }

   string[4] = '\0';
   if (queue_counter == 0)
   {
      string[0] = string[1] = string[2] = ' ';
      string[3] = '0';
   }
   else
   {
      if (queue_counter < 10)
      {
         string[0] = string[1] = string[2] = ' ';
         string[3] = queue_counter + '0';
      }
      else if (queue_counter < 100)
           {
              string[0] = string[1] = ' ';
              string[2] = (queue_counter / 10) + '0';
              string[3] = (queue_counter % 10) + '0';
           }
      else if (queue_counter < 1000)
           {
              string[0] = ' ';
              string[1] = (queue_counter / 100) + '0';
              string[2] = ((queue_counter / 10) % 10) + '0';
              string[3] = (queue_counter % 10) + '0';
           }
           else
           {
              string[0] = ((queue_counter / 1000) % 10) + '0';
              string[1] = ((queue_counter / 100) % 10) + '0';
              string[2] = ((queue_counter / 10) % 10) + '0';
              string[3] = (queue_counter % 10) + '0';
           }
   }

   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, button_window, color_letter_gc,
                    window_width - DEFAULT_FRAME_SPACE - (4 * glyph_width),
                    text_offset + SPACE_ABOVE_LINE + 1,
                    string,
                    4);

   return;
}


/*+++++++++++++++++++++++++ draw_proc_stat() ++++++++++++++++++++++++++++*/
void
draw_proc_stat(int pos, int job_no, int x, int y)
{
   int       offset;
   char      string[3];
   XGCValues gc_values;

   offset = job_no * (button_width + BUTTON_SPACING);

   string[2] = '\0';
   if (connect_data[pos].no_of_files[job_no] > -1)
   {
      int num = connect_data[pos].no_of_files[job_no] % 100;

      string[0] = ((num / 10) % 10) + '0';
      string[1] = (num % 10) + '0';
   }
   else
   {
      string[0] = '0';
      string[1] = '0';
   }

   /* Change color of letters when background color is to dark */
   if ((connect_data[pos].connect_status[job_no] == FTP_ACTIVE) ||
#ifdef _WITH_SCP1_SUPPORT
       (connect_data[pos].connect_status[job_no] == SCP1_ACTIVE) ||
#endif
       (connect_data[pos].connect_status[job_no] == CONNECTING))
   {
      gc_values.foreground = color_pool[WHITE];
   }
   else
   {
      gc_values.foreground = color_pool[FG];
   }

   gc_values.background = color_pool[(int)connect_data[pos].connect_status[job_no]];
   XChangeGC(display, color_letter_gc, GCForeground | GCBackground, &gc_values);
   XDrawImageString(display, line_window, color_letter_gc,
                    x + x_offset_proc + offset,
                    y + text_offset + SPACE_ABOVE_LINE,
                    string,
                    2);

   if (connect_data[pos].detailed_selection[job_no] == YES)
   {
      gc_values.foreground = color_pool[DEBUG_MODE];
      XChangeGC(display, color_gc, GCForeground, &gc_values);
      XDrawRectangle(display, line_window, color_gc,
                     x + x_offset_proc + offset - 1,
                     y + SPACE_ABOVE_LINE - 1,
                     button_width + 1,
                     glyph_height + 1);
   }

   return;
}


/*+++++++++++++++++++++ draw_detailed_selection() +++++++++++++++++++++++*/
void
draw_detailed_selection(int pos, int job_no)
{
   int       x, y, offset;
   XGCValues gc_values;

   offset = job_no * (button_width + BUTTON_SPACING);

   if (connect_data[pos].detailed_selection[job_no] == YES)
   {
      gc_values.foreground = color_pool[DEBUG_MODE];
   }
   else
   {
      if (connect_data[pos].inverse == OFF)
      {
         gc_values.foreground = color_pool[DEFAULT_BG];
      }
      else if (connect_data[pos].inverse == ON)
           {
              gc_values.foreground = color_pool[BLACK];
           }
           else
           {
              gc_values.foreground = color_pool[LOCKED_INVERSE];
           }
   }
   XChangeGC(display, color_gc, GCForeground, &gc_values);
   locate_xy_column(connect_data[pos].long_pos, &x, &y, NULL);
   XDrawRectangle(display, line_window, color_gc,
                  x + x_offset_proc + offset - 1,
                  y + SPACE_ABOVE_LINE - 1,
                  button_width + 1,
                  glyph_height + 1);

   return;
}


/*+++++++++++++++++++++++++++++ draw_chars() ++++++++++++++++++++++++++++*/
void
draw_chars(int pos, char type, int x, int y, int column)
{
   int       length;
   char      *ptr = NULL;
   XGCValues gc_values;
   GC        tmp_gc;

   switch(type)
   {
      case NO_OF_FILES : 
         ptr = connect_data[pos].str_tfc;
         length = 4;
         break;

      case TOTAL_FILE_SIZE : 
         ptr = connect_data[pos].str_tfs;
         length = 4;
         break;

      case TRANSFER_RATE : 
         ptr = connect_data[pos].str_tr;
         length = 4;
         break;

      case ERROR_COUNTER : 
         ptr = connect_data[pos].str_ec;
         length = 2;
         break;

      default : /* That's not possible! */
         (void)xrec(appshell, ERROR_DIALOG,
                    "Unknown character type %d. (%s %d)",
                    (int)type, __FILE__, __LINE__);
         return;
   }

   if (connect_data[pos].inverse > OFF)
   {
      if (connect_data[pos].inverse == ON)
      {
         tmp_gc = normal_letter_gc;
      }
      else
      {
         tmp_gc = locked_letter_gc;
      }
   }
   else
   {
      gc_values.foreground = color_pool[BLACK];
      gc_values.background = color_pool[CHAR_BACKGROUND];
      XChangeGC(display, color_letter_gc, GCForeground | GCBackground,
                &gc_values);
      tmp_gc = color_letter_gc;
   }
   XDrawImageString(display, line_window, tmp_gc,
                    x + x_offset_characters - (max_line_length - line_length[column]),
                    y + text_offset + SPACE_ABOVE_LINE,
                    ptr,
                    length);

   return;
}


/*+++++++++++++++++++++++++++++ draw_bar() ++++++++++++++++++++++++++++++*/
void
draw_bar(int         pos,
         signed char delta,
         char        bar_no,
         int         x,
         int         y,
         int         column)
{
   int x_offset,
       y_offset;

   x_offset = x + x_offset_bars - (max_line_length - line_length[column]);
   y_offset = y + SPACE_ABOVE_LINE;

   if (bar_no == TR_BAR_NO)  /* TRANSFER RATE */
   {
      XFillRectangle(display, line_window, tr_bar_gc, x_offset, y_offset,
                     connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_2);
   }
   else if (bar_no == ERROR_BAR_NO) /* ERROR */
        {
           unsigned long pix;
           XColor        color;
           XGCValues     gc_values;

           color.blue = 0;
           color.green = connect_data[pos].green_color_offset;
           color.red = connect_data[pos].red_color_offset;
           if (XAllocColor(display, default_cmap, &color) == 0)
           {
              pix = color_pool[BLACK];
           }
           else
           {
              pix = color.pixel;
           }

           gc_values.foreground = pix;
           XChangeGC(display, color_gc, GCForeground, &gc_values);
           XFillRectangle(display, line_window, color_gc, x_offset, y_offset,
                          connect_data[pos].bar_length[(int)bar_no],
                          bar_thickness_2);
        }


   /* Remove color behind shrunken bar */
   if (delta < 0)
   {
      GC tmp_gc;

      if (connect_data[pos].inverse == OFF)
      {
         tmp_gc = default_bg_gc;
      }
      else
      {
         if (connect_data[pos].inverse == ON)
         {
            tmp_gc = normal_bg_gc;
         }
         else
         {
            tmp_gc = locked_bg_gc;
         }
      }
      XFillRectangle(display, line_window, tmp_gc,
                     x_offset + connect_data[pos].bar_length[(int)bar_no],
                     y_offset,
                     (int)max_bar_length - connect_data[pos].bar_length[(int)bar_no],
                     bar_thickness_2);
   }

   return;
}
