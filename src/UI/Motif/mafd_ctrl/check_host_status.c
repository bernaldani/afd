/*
 *  check_host_status.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1996 - 2012 Deutscher Wetterdienst (DWD),
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
 **   check_host_status - checks the status of each connection
 **
 ** SYNOPSIS
 **   void check_host_status(Widget w)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.01.1996 H.Kiehl Created
 **   30.08.1997 H.Kiehl Remove sprintf() from critical areas.
 **   22.12.2001 H.Kiehl Added variable column length.
 **   26.12.2001 H.Kiehl Allow for more changes in line style.
 **   21.06.2007 H.Kiehl Split second LED in two halfs to show the
 **                      transfer direction.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strcmp(), strcpy(), memmove()        */
#include <stdlib.h>              /* calloc(), realloc(), free()          */
#include <math.h>                /* log10()                              */
#include <time.h>                /* time()                               */
#include <sys/times.h>           /* times()                              */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "mafd_ctrl.h"

extern Display                    *display;
extern XtAppContext               app;
extern XtIntervalId               interval_id_tv;
extern GC                         color_gc;
extern Widget                     transviewshell;
extern Window                     line_window,
                                  short_line_window;
extern Pixmap                     line_pixmap,
                                  short_line_pixmap;
extern char                       line_style,
                                  tv_window;
extern unsigned char              *p_feature_flag,
                                  saved_feature_flag;
extern float                      max_bar_length;
extern size_t                     current_jd_size;
extern int                        no_of_hosts,
                                  no_of_jobs_selected,
                                  glyph_height,
                                  glyph_width,
                                  led_width,
                                  bar_thickness_2,
                                  *line_length,
                                  max_line_length,
                                  no_selected,
                                  no_of_columns,
                                  no_of_long_lines,
                                  no_of_short_lines,
                                  no_of_rows,
                                  button_width,
                                  tv_no_of_rows,
                                  window_width,
                                  x_offset_proc;
extern unsigned short             step_size;
extern unsigned long              color_pool[],
                                  redraw_time_host;
extern clock_t                    clktck;
extern struct job_data            *jd;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;

/* Local function prototypes. */
static void                       calc_transfer_rate(int, clock_t);
static int                        check_fsa_data(unsigned int),
                                  check_disp_data(unsigned int, int);


/*######################### check_host_status() #########################*/
void
check_host_status(Widget w)
{
   unsigned char new_color;
   signed char   flush;
   int           column,
                 disable_retrieve_flag_changed,
                 i,
                 j,
                 x,
                 y,
                 pos,
                 prev_no_of_hosts = no_of_hosts,
                 led_changed = 0,
                 location_where_changed,
                 new_bar_length,
                 old_bar_length,
                 redraw_everything = NO;
   clock_t       end_time;
   time_t        current_time;
   double        tmp_transfer_rate;
   struct tms    tmsdummy;

   /* Initialise variables. */
   location_where_changed = no_of_hosts + 10;
   flush = NO;
   if (*(unsigned char *)((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END) & DISABLE_HOST_WARN_TIME)
   {
      current_time = 0L;
   }
   else
   {
      current_time = time(NULL);
   }

   /*
    * See if a host has been added or removed from the FSA.
    * If it changed resize the window.
    */
   if (check_fsa(NO) == YES)
   {
      int         nll = 0,    /* Number of long lines. */
                  nsl = 0;    /* Number of short lines. */
      size_t      new_size = (no_of_hosts + 1) * sizeof(struct line);
      struct line *new_connect_data,
                  *tmp_connect_data;

      p_feature_flag = (unsigned char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
      if ((new_connect_data = calloc((no_of_hosts + 1),
                                     sizeof(struct line))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }

      /*
       * First try to copy the connect data from the old structure
       * so long as the hostnames are the same.
       */
      for (i = 0, location_where_changed = 0; i < prev_no_of_hosts; i++, location_where_changed++)
      {
         if (connect_data[i].host_id == fsa[i].host_id)
         {
            (void)memcpy(&new_connect_data[i], &connect_data[i],
                         sizeof(struct line));
         }
         else
         {
            break;
         }
      }

      end_time = times(&tmsdummy);
      for (i = location_where_changed; i < no_of_hosts; i++)
      {
         if ((pos = check_disp_data(fsa[i].host_id, prev_no_of_hosts)) != INCORRECT)
         {
            (void)memcpy(&new_connect_data[i], &connect_data[pos],
                         sizeof(struct line));
         }
         else /* A new host has been added. */
         {
            /* Initialise values for new host. */
            (void)strcpy(new_connect_data[i].hostname, fsa[i].host_alias);
            new_connect_data[i].host_id = fsa[i].host_id;
            (void)sprintf(new_connect_data[i].host_display_str, "%-*s",
                          MAX_HOSTNAME_LENGTH, fsa[i].host_dsp_name);
            if (fsa[i].host_toggle_str[0] != '\0')
            {
               new_connect_data[i].host_toggle_display = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            }
            else
            {
               new_connect_data[i].host_toggle_display = fsa[i].host_dsp_name[(int)fsa[i].toggle_pos];
            }
            new_connect_data[i].host_status = fsa[i].host_status;
            new_connect_data[i].special_flag = fsa[i].special_flag;
            new_connect_data[i].start_event_handle = fsa[i].start_event_handle;
            new_connect_data[i].end_event_handle = fsa[i].end_event_handle;
            if (new_connect_data[i].special_flag & HOST_DISABLED)
            {
               new_connect_data[i].stat_color_no = WHITE;
            }
            else if ((new_connect_data[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
                 {
                    new_connect_data[i].stat_color_no = DEFAULT_BG;
                 }
            else if (fsa[i].error_counter >= fsa[i].max_errors)
                 {
                    if ((new_connect_data[i].host_status & HOST_ERROR_OFFLINE) ||
                        ((new_connect_data[i].host_status & HOST_ERROR_OFFLINE_T) &&
                         ((new_connect_data[i].start_event_handle == 0) ||
                          (current_time >= new_connect_data[i].start_event_handle)) &&
                         ((new_connect_data[i].end_event_handle == 0) ||
                          (current_time <= new_connect_data[i].end_event_handle))) ||
                        (new_connect_data[i].host_status & HOST_ERROR_OFFLINE_STATIC))
                    {
                       new_connect_data[i].stat_color_no = ERROR_OFFLINE_ID;
                    }
                    else if ((new_connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED) ||
                             ((new_connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                              ((new_connect_data[i].start_event_handle == 0) ||
                               (current_time >= new_connect_data[i].start_event_handle)) &&
                              ((new_connect_data[i].end_event_handle == 0) ||
                               (current_time <= new_connect_data[i].end_event_handle))))
                         {
                            new_connect_data[i].stat_color_no = ERROR_ACKNOWLEDGED_ID;
                         }
                         else
                         {
                            new_connect_data[i].stat_color_no = NOT_WORKING2;
                         }
                 }
            else if (new_connect_data[i].host_status & HOST_WARN_TIME_REACHED)
                 {
                    if ((new_connect_data[i].host_status & HOST_ERROR_OFFLINE) ||
                        ((new_connect_data[i].host_status & HOST_ERROR_OFFLINE_T) &&
                         ((new_connect_data[i].start_event_handle == 0) ||
                          (current_time >= new_connect_data[i].start_event_handle)) &&
                         ((new_connect_data[i].end_event_handle == 0) ||
                          (current_time <= new_connect_data[i].end_event_handle))) ||
                        (new_connect_data[i].host_status & HOST_ERROR_OFFLINE_STATIC))
                    {
                       new_connect_data[i].stat_color_no = ERROR_OFFLINE_ID;
                    }
                    else if ((new_connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED) ||
                             ((new_connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                              ((new_connect_data[i].start_event_handle == 0) ||
                               (current_time >= new_connect_data[i].start_event_handle)) &&
                              ((new_connect_data[i].end_event_handle == 0) ||
                               (current_time <= new_connect_data[i].end_event_handle))))
                         {
                            new_connect_data[i].stat_color_no = ERROR_ACKNOWLEDGED_ID;
                         }
                         else
                         {
                            new_connect_data[i].stat_color_no = WARNING_ID;
                         }
                 }
            else if (fsa[i].active_transfers > 0)
                 {
                    new_connect_data[i].stat_color_no = TRANSFER_ACTIVE; /* Transferring files. */
                 }
                 else
                 {
                    new_connect_data[i].stat_color_no = NORMAL_STATUS;
                 }
            new_connect_data[i].debug = fsa[i].debug;
            new_connect_data[i].start_time = end_time;
            new_connect_data[i].total_file_counter = fsa[i].total_file_counter;
            CREATE_FC_STRING(new_connect_data[i].str_tfc, new_connect_data[i].total_file_counter);
            new_connect_data[i].total_file_size = fsa[i].total_file_size;
            CREATE_FS_STRING(new_connect_data[i].str_tfs, new_connect_data[i].total_file_size);
            new_connect_data[i].bytes_per_sec = 0;
            new_connect_data[i].str_tr[0] = new_connect_data[i].str_tr[1] = ' ';
            new_connect_data[i].str_tr[2] = '0'; new_connect_data[i].str_tr[3] = 'B';
            new_connect_data[i].str_tr[4] = '\0';
            new_connect_data[i].average_tr = 0.0;
            new_connect_data[i].max_average_tr = 0.0;
            new_connect_data[i].max_errors = fsa[i].max_errors;
            new_connect_data[i].error_counter = fsa[i].error_counter;
            new_connect_data[i].protocol = fsa[i].protocol;
            if (new_connect_data[i].host_status & PAUSE_QUEUE_STAT)
            {
               new_connect_data[i].status_led[0] = PAUSE_QUEUE;
            }
            else if ((new_connect_data[i].host_status & AUTO_PAUSE_QUEUE_STAT) ||
                     (new_connect_data[i].host_status & DANGER_PAUSE_QUEUE_STAT))
                 {
                    new_connect_data[i].status_led[0] = AUTO_PAUSE_QUEUE;
                 }
#ifdef WITH_ERROR_QUEUE
            else if (new_connect_data[i].host_status & ERROR_QUEUE_SET)
                 {
                    new_connect_data[i].status_led[0] = JOBS_IN_ERROR_QUEUE;
                 }
#endif
                 else
                 {
                    new_connect_data[i].status_led[0] = NORMAL_STATUS;
                 }
            if (new_connect_data[i].host_status & STOP_TRANSFER_STAT)
            {
               new_connect_data[i].status_led[1] = STOP_TRANSFER;
            }
            else
            {
               new_connect_data[i].status_led[1] = NORMAL_STATUS;
            }
            new_connect_data[i].status_led[2] = (new_connect_data[i].protocol >> 30);
            CREATE_EC_STRING(new_connect_data[i].str_ec, new_connect_data[i].error_counter);
            if (new_connect_data[i].max_errors < 2)
            {
               new_connect_data[i].scale = (double)max_bar_length;
            }
            else
            {
               new_connect_data[i].scale = max_bar_length / new_connect_data[i].max_errors;
            }
            new_connect_data[i].bar_length[TR_BAR_NO] = 0;
            new_connect_data[i].bar_length[ERROR_BAR_NO] = 0;
            new_connect_data[i].inverse = OFF;
            new_connect_data[i].allowed_transfers = fsa[i].allowed_transfers;
            for (j = 0; j < new_connect_data[i].allowed_transfers; j++)
            {
               new_connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;
               new_connect_data[i].bytes_send[j] = fsa[i].job_status[j].bytes_send;
               new_connect_data[i].detailed_selection[j] = NO;
               if (fsa[i].job_status[j].connect_status != 0)
               {
                  new_connect_data[i].connect_status[j] = fsa[i].job_status[j].connect_status;
               }
               else
               {
                  new_connect_data[i].connect_status[j] = WHITE;
               }
            }
            new_connect_data[i].short_pos = -1;
            new_connect_data[i].long_pos = i;
            for (j = 0; j < i; j++)
            {
               if (new_connect_data[j].long_pos == -1)
               {
                  new_connect_data[i].long_pos--;
               }
            }
            no_of_long_lines++;
         }
      }

      /*
       * Ensure that we really have checked all hosts in old
       * structure.
       */
      for (i = 0; i < prev_no_of_hosts; i++)
      {
         if (check_fsa_data(connect_data[i].host_id) == INCORRECT)
         {
            if (connect_data[i].long_pos == -1)
            {
               no_of_short_lines--;
            }
            else
            {
               no_of_long_lines--;
            }
            if (connect_data[i].inverse == ON)
            {
               /* Host has been deleted. */
               no_selected--;
            }
         }
      }

      /*
       * Ensure that the positions of all long and short lines
       * are still correct. It could be that the host order
       * has been changed in the HOST_CONFIG file.
       */
      for (i = 0; i < no_of_hosts; i++)
      {
         if (new_connect_data[i].long_pos == -1)
         {
            new_connect_data[i].short_pos = nsl;
            nsl++;
         }
         else
         {
            new_connect_data[i].long_pos = nll;
            nll++;
         }
      }

      if ((tmp_connect_data = realloc(connect_data, new_size)) == NULL)
      {
         int tmp_errno = errno;

         free(connect_data);
         (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                    strerror(tmp_errno), __FILE__, __LINE__);
         return;
      }
      connect_data = tmp_connect_data;

      /* Activate the new connect_data structure. */
      (void)memcpy(&connect_data[0], &new_connect_data[0],
                   no_of_hosts * sizeof(struct line));

      free(new_connect_data);

      /* Resize window if necessary. */
      if ((redraw_everything = resize_window()) == YES)
      {
         if (no_of_columns != 0)
         {
            location_where_changed = 0;
         }
      }

      /* When no. of channels have been reduced, then delete */
      /* removed channels from end of list.                  */
      if (no_of_columns > 1)
      {
         for (i = prev_no_of_hosts; i > no_of_hosts; i--)
         {
            draw_blank_line(i - 1);
         }
      }

      /*
       * Change the detailed transfer window if it is active.
       */
      if (no_of_jobs_selected > 0)
      {
         int             new_no_of_jobs_selected = 0;
         size_t          new_current_jd_size = 0;
         struct job_data *new_jd = NULL;

         for (i = 0; i < no_of_hosts; i++)
         {
            for (j = 0; j < connect_data[i].allowed_transfers; j++)
            {
               if (connect_data[i].detailed_selection[j] == YES)
               {
                  new_no_of_jobs_selected++;
                  if (new_no_of_jobs_selected == 1)
                  {
                     new_size = 5 * sizeof(struct job_data);

                     new_current_jd_size = new_size;
                     if ((new_jd = malloc(new_size)) == NULL)
                     {
                        (void)xrec(FATAL_DIALOG,
                                   "malloc() error [%d] : %s [%d] (%s %d)",
                                   new_size, strerror(errno), errno,
                                   __FILE__, __LINE__);
                        no_of_jobs_selected = 0;
                        return;
                     }
                  }
                  else if ((new_no_of_jobs_selected % 5) == 0)
                       {
                          new_size = ((new_no_of_jobs_selected / 5) + 1) *
                                     5 * sizeof(struct job_data);

                          if (new_current_jd_size < new_size)
                          {
                             struct job_data *tmp_new_jd;

                             new_current_jd_size = new_size;
                             if ((tmp_new_jd = realloc(new_jd, new_size)) == NULL)
                             {
                                int tmp_errno = errno;

                                free(new_jd);
                                (void)xrec(FATAL_DIALOG,
                                           "realloc() error [%d] : %s [%d] (%s %d)",
                                           new_size, strerror(tmp_errno),
                                           tmp_errno, __FILE__, __LINE__);
                                no_of_jobs_selected = 0;
                                return;
                             }
                             new_jd = tmp_new_jd;
                          }
                       }
                  init_jd_structure(&new_jd[new_no_of_jobs_selected - 1], i, j);
               }
            }
         } /* for (i = 0; i < no_of_hosts; i++) */

         if (new_no_of_jobs_selected > 0)
         {
            new_size = new_no_of_jobs_selected * sizeof(struct job_data);

            if (new_no_of_jobs_selected != no_of_jobs_selected)
            {
               no_of_jobs_selected = new_no_of_jobs_selected;

               if (new_current_jd_size > current_jd_size)
               {
                  struct job_data *tmp_jd;

                  current_jd_size = new_current_jd_size;
                  if ((tmp_jd = realloc(jd, new_size)) == NULL)
                  {
                     int tmp_errno = errno;

                     free(jd);
                     (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                                strerror(tmp_errno), __FILE__, __LINE__);
                     no_of_jobs_selected = 0;
                     return;
                  }
                  jd = tmp_jd;
               }

               (void)resize_tv_window();
            }
            if (new_jd != NULL)
            {
               (void)memcpy(jd, new_jd, new_size);
               free(new_jd);
            }

            for (i = 0; i < no_of_jobs_selected; i++)
            {
               draw_detailed_line(i);
            }
         }
         else
         {
            no_of_jobs_selected = 0;
            XtRemoveTimeOut(interval_id_tv);
            free(jd);
            jd = NULL;
            XtPopdown(transviewshell);
         }
      } /* if (no_of_jobs_selected > 0) */

      /* Make sure changes are drawn!!! */
      flush = YES;
   } /* if (check_fsa(NO) == YES) */

   if ((line_style & SHOW_CHARACTERS) || (line_style & SHOW_BARS))
   {
      end_time = times(&tmsdummy);
   }

   if (*p_feature_flag != saved_feature_flag)
   {
      if (((saved_feature_flag & DISABLE_RETRIEVE) &&
           (*p_feature_flag & DISABLE_RETRIEVE)) ||
          (((saved_feature_flag & DISABLE_RETRIEVE) == 0) &&
           ((*p_feature_flag & DISABLE_RETRIEVE) == 0)))
      {
         disable_retrieve_flag_changed = NO;
      }
      else
      {
         disable_retrieve_flag_changed = YES;
      }
      saved_feature_flag = *p_feature_flag;
   }
   else
   {
      disable_retrieve_flag_changed = NO;
   }

   /* Change information for each remote host if necessary. */
   for (i = 0; i < no_of_hosts; i++)
   {
      x = y = -1;

      if ((connect_data[i].short_pos == -1) && (line_style & SHOW_JOBS))
      {
         if (connect_data[i].allowed_transfers != fsa[i].allowed_transfers)
         {
            int column_length,
                max_no_parallel_jobs,
                row_counter;

            locate_xy_column(connect_data[i].long_pos, &x, &y, &column);

            /*
             * Lets determine if this does change the column length.
             */
            max_no_parallel_jobs = 0;
            row_counter = column * no_of_rows;
            for (j = 0; j < no_of_rows; j++)
            {
               if (max_no_parallel_jobs < fsa[row_counter].allowed_transfers)
               {
                  max_no_parallel_jobs = fsa[row_counter].allowed_transfers;
               }
               row_counter++;
            }
            column_length = max_line_length - (((MAX_NO_PARALLEL_JOBS - max_no_parallel_jobs) * (button_width + BUTTON_SPACING)) - BUTTON_SPACING);
            if (line_length[column] != column_length)
            {
               /*
                * Column length has changed! We now need to redraw the
                * whole window.
                */
               line_length[column] = column_length;

               /* Resize window if necessary. */
               redraw_everything = resize_window();
            }
            else
            {
               if (connect_data[i].allowed_transfers < fsa[i].allowed_transfers)
               {
                  for (j = connect_data[i].allowed_transfers; j < fsa[i].allowed_transfers; j++)
                  {
                     draw_proc_stat(i, j, x, y);
                  }
               }
               else
               {
                  int       offset;
                  XGCValues gc_values;

                  for (j = fsa[i].allowed_transfers; j < connect_data[i].allowed_transfers; j++)
                  {
                     offset = j * (button_width + BUTTON_SPACING);

                     if (connect_data[i].inverse == OFF)
                     {
                        gc_values.foreground = color_pool[DEFAULT_BG];
                     }
                     else if (connect_data[i].inverse == ON)
                          {
                             gc_values.foreground = color_pool[BLACK];
                          }
                          else
                          {
                             gc_values.foreground = color_pool[LOCKED_INVERSE];
                          }
                     XChangeGC(display, color_gc, GCForeground, &gc_values);
                     XFillRectangle(display, line_window, color_gc,
                                    x + x_offset_proc + offset - 1,
                                    y + SPACE_ABOVE_LINE - 1,
                                    button_width + 2,
                                    glyph_height + 2);
                     XFillRectangle(display, line_pixmap, color_gc,
                                    x + x_offset_proc + offset - 1,
                                    y + SPACE_ABOVE_LINE - 1,
                                    button_width + 2,
                                    glyph_height + 2);

                     /* Update detailed selection. */
                     if ((no_of_jobs_selected > 0) &&
                         (connect_data[i].detailed_selection[j] == YES))
                     {
                        no_of_jobs_selected--;
                        connect_data[i].detailed_selection[j] = NO;
                        if (no_of_jobs_selected == 0)
                        {
                           XtRemoveTimeOut(interval_id_tv);
                           free(jd);
                           jd = NULL;
                           XtPopdown(transviewshell);
                           tv_window = OFF;
                        }
                        else
                        {
                           int k, m, tmp_tv_no_of_rows;

                           /* Remove detailed selection. */
                           for (k = 0; k < (no_of_jobs_selected + 1); k++)
                           {
                              if ((jd[k].job_no == j) &&
                                  (jd[k].host_id == connect_data[i].host_id))
                              {
                                 if (k != no_of_jobs_selected)
                                 {
                                    size_t move_size = (no_of_jobs_selected - k) * sizeof(struct job_data);

                                    (void)memmove(&jd[k], &jd[k + 1], move_size);
                                 }
                                 break;
                              }
                           }

                           for (m = k; m < no_of_jobs_selected; m++)
                           {
                              draw_detailed_line(m);
                           }

                           tmp_tv_no_of_rows = tv_no_of_rows;
                           if (resize_tv_window() == YES)
                           {
                              for (k = (tmp_tv_no_of_rows - 1); k < no_of_jobs_selected; k++)
                              {
                                 draw_detailed_line(k);
                              }
                           }

                           draw_tv_blank_line(m);
                           XFlush(display);
                        }
                     }
                  }
               }
            }
            connect_data[i].allowed_transfers = fsa[i].allowed_transfers;
            flush = YES;
         }

         /* For each transfer, see if number of files changed. */
         for (j = 0; j < fsa[i].allowed_transfers; j++)
         {
            if (connect_data[i].connect_status[j] != fsa[i].job_status[j].connect_status)
            {
               connect_data[i].connect_status[j] = fsa[i].job_status[j].connect_status;

               if (connect_data[i].no_of_files[j] != (fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done))
               {
                  connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;
               }

               locate_xy_column(connect_data[i].long_pos, &x, &y, &column);

               /* Draw changed process file counter button. */
               draw_proc_stat(i, j, x, y);

               flush = YES;
            }
            else if (connect_data[i].no_of_files[j] != (fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done))
                 {
                    connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;

                    locate_xy_column(connect_data[i].long_pos, &x, &y, &column);

                    /* Draw changed process file counter button. */
                    draw_proc_stat(i, j, x, y);

                    flush = YES;
                 }
         }
      }

      if (connect_data[i].max_errors != fsa[i].max_errors)
      {
         connect_data[i].max_errors = fsa[i].max_errors;

         /*
          * Hmmm. What now? We cannot do anything here since
          * we cannot assume that the afd_ctrl is always
          * running.
          */
      }

      if (connect_data[i].special_flag != fsa[i].special_flag)
      {
         connect_data[i].special_flag = fsa[i].special_flag;
      }
      if (connect_data[i].host_status != fsa[i].host_status)
      {
         connect_data[i].host_status = fsa[i].host_status;
      }
      if (connect_data[i].protocol != fsa[i].protocol)
      {
         connect_data[i].protocol = fsa[i].protocol;
      }

      /* Did any significant change occur in the status */
      /* for this host?                                 */
      if (connect_data[i].special_flag & HOST_DISABLED)
      {
         new_color = WHITE;
      }
      else if ((connect_data[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
           {
              new_color = DEFAULT_BG;
           }
      else if (fsa[i].error_counter >= fsa[i].max_errors)
           {
              if ((connect_data[i].host_status & HOST_ERROR_OFFLINE) ||
                  ((connect_data[i].host_status & HOST_ERROR_OFFLINE_T) &&
                   ((connect_data[i].start_event_handle == 0) ||
                    (current_time >= connect_data[i].start_event_handle)) &&
                   ((connect_data[i].end_event_handle == 0) ||
                    (current_time <= connect_data[i].end_event_handle))) ||
                  (connect_data[i].host_status & HOST_ERROR_OFFLINE_STATIC))
              {
                 new_color = ERROR_OFFLINE_ID;
              }
              else if ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED) ||
                       ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                        ((connect_data[i].start_event_handle == 0) ||
                         (current_time >= connect_data[i].start_event_handle)) &&
                        ((connect_data[i].end_event_handle == 0) ||
                         (current_time <= connect_data[i].end_event_handle))))
                   {
                      new_color = ERROR_ACKNOWLEDGED_ID;
                   }
                   else
                   {
                      new_color = NOT_WORKING2;
                   }
           }
      else if (connect_data[i].host_status & HOST_WARN_TIME_REACHED)
           {
              if ((connect_data[i].host_status & HOST_ERROR_OFFLINE) ||
                  ((connect_data[i].host_status & HOST_ERROR_OFFLINE_T) &&
                   ((connect_data[i].start_event_handle == 0) ||
                    (current_time >= connect_data[i].start_event_handle)) &&
                   ((connect_data[i].end_event_handle == 0) ||
                    (current_time <= connect_data[i].end_event_handle))) ||
                  (connect_data[i].host_status & HOST_ERROR_OFFLINE_STATIC))
              {
                 new_color = ERROR_OFFLINE_ID;
              }
              else if ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED) ||
                       ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                        ((connect_data[i].start_event_handle == 0) ||
                         (current_time >= connect_data[i].start_event_handle)) &&
                        ((connect_data[i].end_event_handle == 0) ||
                         (current_time <= connect_data[i].end_event_handle))))
                   {
                      new_color = ERROR_ACKNOWLEDGED_ID;
                   }
                   else
                   {
                      new_color = WARNING_ID;
                   }
           }
      else if (fsa[i].active_transfers > 0)
           {
              new_color = TRANSFER_ACTIVE; /* Transferring files. */
           }
           else
           {
              new_color = NORMAL_STATUS; /* Nothing to do but connection active. */
           }

      if (connect_data[i].stat_color_no != new_color)
      {
         connect_data[i].stat_color_no = new_color;

         if ((i < location_where_changed) && (redraw_everything == NO))
         {
            if (connect_data[i].short_pos == -1)
            {
               if (x == -1)
               {
                  locate_xy_column(connect_data[i].long_pos,
                                   &x, &y, &column);
               }
               draw_dest_identifier(line_window, line_pixmap, i, x, y);
            }
            else
            {
               if (x == -1)
               {
                  locate_xy_short(connect_data[i].short_pos, &x, &y);
               }
               draw_dest_identifier(short_line_window, short_line_pixmap, i,
                                    x, y);
            }
            flush = YES;
         }
      }

      /* Host switched? */
      if (connect_data[i].host_toggle != fsa[i].host_toggle)
      {
         connect_data[i].host_toggle = fsa[i].host_toggle;

         if (fsa[i].host_toggle_str[0] != '\0')
         {
            connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            connect_data[i].host_toggle_display = connect_data[i].host_display_str[(int)fsa[i].toggle_pos];

            if ((i < location_where_changed) && (redraw_everything == NO))
            {
               if (connect_data[i].short_pos == -1)
               {
                  if (x == -1)
                  {
                     locate_xy_column(connect_data[i].long_pos,
                                      &x, &y, &column);
                  }
                  draw_dest_identifier(line_window, line_pixmap, i, x, y);
               }
               else
               {
                  if (x == -1)
                  {
                     locate_xy_short(connect_data[i].short_pos, &x, &y);
                  }
                  draw_dest_identifier(short_line_window, short_line_pixmap, i,
                                       x, y);
               }
               flush = YES;
            }

            /* Don't forget to redraw display name of tv window. */
            if (no_of_jobs_selected > 0)
            {
               int ii;

               for (ii = 0; ii < no_of_jobs_selected; ii++)
               {
                  if (jd[ii].fsa_no == i)
                  {
                     int x, y;

                     while ((ii < no_of_jobs_selected) && (jd[ii].fsa_no == i))
                     {
                        jd[ii].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
                        tv_locate_xy(ii, &x, &y);
                        draw_tv_dest_identifier(ii, x, y);
                        ii++;
                     }
                     break;
                  }
               }
            }
         }
      }

      /* Did the toggle information change? */
      if (connect_data[i].host_toggle_display != fsa[i].host_dsp_name[(int)fsa[i].toggle_pos])
      {
         connect_data[i].host_toggle_display = fsa[i].host_dsp_name[(int)fsa[i].toggle_pos];

         if (fsa[i].host_toggle_str[0] != '\0')
         {
            connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
         }
         else
         {
            connect_data[i].host_display_str[(int)fsa[i].toggle_pos] = ' ';
         }

         if ((i < location_where_changed) && (redraw_everything == NO))
         {
            if (connect_data[i].short_pos == -1)
            {
               if (x == -1)
               {
                  locate_xy_column(connect_data[i].long_pos, &x, &y, &column);
               }
               draw_dest_identifier(line_window, line_pixmap, i, x, y);
            }
            else
            {
               if (x == -1)
               {
                  locate_xy_short(connect_data[i].short_pos, &x, &y);
               }
               draw_dest_identifier(short_line_window, short_line_pixmap, i,
                                    x, y);
            }
            flush = YES;
         }

         /* Don't forget to redraw display name of tv window. */
         if (no_of_jobs_selected > 0)
         {
            int ii;

            for (ii = 0; ii < no_of_jobs_selected; ii++)
            {
               if (jd[ii].fsa_no == i)
               {
                  int x, y;

                  while ((ii < no_of_jobs_selected) && (jd[ii].fsa_no == i))
                  {
                     jd[ii].host_display_str[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
                     tv_locate_xy(ii, &x, &y);
                     draw_tv_dest_identifier(ii, x, y);
                     ii++;
                  }
                  break;
               }
            }
         }
      }

      if (connect_data[i].short_pos == -1)
      {
         /*
          * LED INFORMATION
          * ===============
          */
         if (line_style & SHOW_LEDS)
         {
            /*
             * DEBUG LED
             * =========
             */
            if (connect_data[i].debug != fsa[i].debug)
            {
               connect_data[i].debug = fsa[i].debug;

               if ((i < location_where_changed) && (redraw_everything == NO))
               {
                  if (x == -1)
                  {
                     locate_xy_column(connect_data[i].long_pos,
                                      &x, &y, &column);
                  }
                  draw_debug_led(i, x, y);
                  flush = YES;
               }
            }

            /*
             * STATUS LED
             * ==========
             */
            if (connect_data[i].host_status & PAUSE_QUEUE_STAT)
            {
               if (connect_data[i].status_led[0] != PAUSE_QUEUE)
               {
                  connect_data[i].status_led[0] = PAUSE_QUEUE;
                  led_changed |= LED_ONE;
               }
            }
            else if ((connect_data[i].host_status & AUTO_PAUSE_QUEUE_STAT) ||
                     (connect_data[i].host_status & DANGER_PAUSE_QUEUE_STAT))
                 {
                    if (connect_data[i].status_led[0] != AUTO_PAUSE_QUEUE)
                    {
                       connect_data[i].status_led[0] = AUTO_PAUSE_QUEUE;
                       led_changed |= LED_ONE;
                    }
                 }
#ifdef WITH_ERROR_QUEUE
            else if (connect_data[i].host_status & ERROR_QUEUE_SET)
                 {
                    if (connect_data[i].status_led[0] != JOBS_IN_ERROR_QUEUE)
                    {
                       connect_data[i].status_led[0] = JOBS_IN_ERROR_QUEUE;
                       led_changed |= LED_ONE;
                    }
                 }
#endif
                 else
                 {
                    if (connect_data[i].status_led[0] != NORMAL_STATUS)
                    {
                       connect_data[i].status_led[0] = NORMAL_STATUS;
                       led_changed |= LED_ONE;
                    }
                 }
            if (connect_data[i].host_status & STOP_TRANSFER_STAT)
            {
               if (connect_data[i].status_led[1] != STOP_TRANSFER)
               {
                  connect_data[i].status_led[1] = STOP_TRANSFER;
                  led_changed |= LED_TWO;
               }
            }
            else
            {
               if (connect_data[i].status_led[1] != NORMAL_STATUS)
               {
                  connect_data[i].status_led[1] = NORMAL_STATUS;
                  led_changed |= LED_TWO;
               }
            }
            if (connect_data[i].status_led[2] != (connect_data[i].protocol >> 30))
            {
               connect_data[i].status_led[2] = (connect_data[i].protocol >> 30);
               led_changed |= LED_TWO;
            }
            if ((i < location_where_changed) && (redraw_everything == NO))
            {
               if ((led_changed > 0) || (disable_retrieve_flag_changed == YES))
               {
                  if (x == -1)
                  {
                     locate_xy_column(connect_data[i].long_pos,
                                      &x, &y, &column);
                  }
                  if (led_changed & LED_ONE)
                  {
                     draw_led(i, 0, x, y);
                  }
                  if ((led_changed & LED_TWO) ||
                      (disable_retrieve_flag_changed == YES))
                  {
                     draw_led(i, 1, x + led_width + LED_SPACING, y);
                  }
                  led_changed = 0;
                  flush = YES;
               }
            }
         }

         /*
          * CHARACTER INFORMATION
          * =====================
          *
          * If in character mode see if any change took place,
          * if so, redraw only those characters.
          */
         if (line_style & SHOW_CHARACTERS)
         {
            /*
             * Number of files to be send (NF)
             */
            if (connect_data[i].total_file_counter != fsa[i].total_file_counter)
            {
               if (x == -1)
               {
                  locate_xy_column(connect_data[i].long_pos, &x, &y, &column);
               }

               connect_data[i].total_file_counter = fsa[i].total_file_counter;
               CREATE_FC_STRING(connect_data[i].str_tfc,
                                connect_data[i].total_file_counter);

               if ((i < location_where_changed) && (redraw_everything == NO))
               {
                  draw_chars(i, NO_OF_FILES, x, y, column);
                  flush = YES;
               }
            }

            /*
             * Total File Size (TFS)
             */
            if (connect_data[i].total_file_size != fsa[i].total_file_size)
            {
               char tmp_string[5];

               if (x == -1)
               {
                  locate_xy_column(connect_data[i].long_pos, &x, &y, &column);
               }

               connect_data[i].total_file_size = fsa[i].total_file_size;
               CREATE_FS_STRING(tmp_string, connect_data[i].total_file_size);

               if ((tmp_string[2] != connect_data[i].str_tfs[2]) ||
                   (tmp_string[1] != connect_data[i].str_tfs[1]) ||
                   (tmp_string[0] != connect_data[i].str_tfs[0]) ||
                   (tmp_string[3] != connect_data[i].str_tfs[3]))
               {
                  connect_data[i].str_tfs[0] = tmp_string[0];
                  connect_data[i].str_tfs[1] = tmp_string[1];
                  connect_data[i].str_tfs[2] = tmp_string[2];
                  connect_data[i].str_tfs[3] = tmp_string[3];

                  if ((i < location_where_changed) && (redraw_everything == NO))
                  {
                     draw_chars(i, TOTAL_FILE_SIZE, x + (5 * glyph_width),
                                y, column);
                     flush = YES;
                  }
               }
            }

            /*
             * Transfer Rate (TR)
             */
            tmp_transfer_rate = connect_data[i].bytes_per_sec;
            calc_transfer_rate(i, end_time);

            /* NOTE: We show the actual active transfer rate at this */
            /*       moment. When drawing the bar we show the        */
            /*       average transfer rate.                          */
            /*       ^^^^^^^                                         */
            if (connect_data[i].bytes_per_sec != tmp_transfer_rate)
            {
               char tmp_string[5];

               if (x == -1)
               {
                  locate_xy_column(connect_data[i].long_pos, &x, &y, &column);
               }

               CREATE_FS_STRING(tmp_string, connect_data[i].bytes_per_sec);

               if ((tmp_string[2] != connect_data[i].str_tr[2]) ||
                   (tmp_string[1] != connect_data[i].str_tr[1]) ||
                   (tmp_string[0] != connect_data[i].str_tr[0]) ||
                   (tmp_string[3] != connect_data[i].str_tr[3]))
               {
                  connect_data[i].str_tr[0] = tmp_string[0];
                  connect_data[i].str_tr[1] = tmp_string[1];
                  connect_data[i].str_tr[2] = tmp_string[2];
                  connect_data[i].str_tr[3] = tmp_string[3];

                  if ((i < location_where_changed) && (redraw_everything == NO))
                  {
                     draw_chars(i, TRANSFER_RATE, x + (10 * glyph_width),
                                y, column);
                     flush = YES;
                  }
               }
            }

            /*
             * Error Counter (EC)
             */
            if (connect_data[i].error_counter != fsa[i].error_counter)
            {
               if (x == -1)
               {
                  locate_xy_column(connect_data[i].long_pos, &x, &y, &column);
               }

               /*
                * If line_style is CHARACTERS and BARS don't update
                * the connect_data structure. Otherwise when we draw the bar
                * we will not notice any change. There we will then update
                * the structure.
                */
               if ((line_style & SHOW_BARS) == 0)
               {
                  connect_data[i].error_counter = fsa[i].error_counter;
               }
               CREATE_EC_STRING(connect_data[i].str_ec, fsa[i].error_counter);

               if ((i < location_where_changed) && (redraw_everything == NO))
               {
                  draw_chars(i, ERROR_COUNTER, x + (15 * glyph_width),
                             y, column);
                  flush = YES;
               }
            }
         } /* if (line_style & SHOW_CHARACTERS) */

         /*
          * BAR INFORMATION
          * ===============
          */
         if (line_style & SHOW_BARS)
         {
            /*
             * Transfer Rate Bar
             */
            /* Did we already calculate the transfer rate? */
            if ((line_style & SHOW_CHARACTERS) == 0)
            {
               calc_transfer_rate(i, end_time);
            }

            if (connect_data[i].average_tr > 1.0)
            {
               if (connect_data[i].max_average_tr < 2.0)
               {
                  new_bar_length = (log10(connect_data[i].average_tr) *
                                    max_bar_length /
                                    log10((double) 2.0));
               }
               else
               {
                  new_bar_length = (log10(connect_data[i].average_tr) *
                                    max_bar_length /
                                    log10(connect_data[i].max_average_tr));
               }
            }
            else
            {
               new_bar_length = 0;
            }

            if ((connect_data[i].bar_length[TR_BAR_NO] != new_bar_length) &&
                (new_bar_length < max_bar_length))
            {
               old_bar_length = connect_data[i].bar_length[TR_BAR_NO];
               connect_data[i].bar_length[TR_BAR_NO] = new_bar_length;

               if ((i < location_where_changed) && (redraw_everything == NO))
               {
                  if (x == -1)
                  {
                     locate_xy_column(connect_data[i].long_pos,
                                      &x, &y, &column);
                  }

                  if (old_bar_length < new_bar_length)
                  {
                     draw_bar(i, 1, TR_BAR_NO, x, y, column);
                  }
                  else
                  {
                     draw_bar(i, -1, TR_BAR_NO, x, y, column);
                  }

                  if (flush != YES)
                  {
                     flush = YUP;
                  }
               }
            }
            else if ((new_bar_length >= max_bar_length) &&
                     (connect_data[i].bar_length[TR_BAR_NO] < max_bar_length))
                 {
                    connect_data[i].bar_length[TR_BAR_NO] = max_bar_length;
                    if ((i < location_where_changed) &&
                        (redraw_everything == NO))
                    {
                       if (x == -1)
                       {
                          locate_xy_column(connect_data[i].long_pos,
                                           &x, &y, &column);
                       }

                       draw_bar(i, 1, TR_BAR_NO, x, y, column);

                       if (flush != YES)
                       {
                          flush = YUP;
                       }
                    }
                 }

            /*
             * Error Bar
             */
            if (connect_data[i].error_counter != fsa[i].error_counter)
            {
               connect_data[i].error_counter = fsa[i].error_counter;
               if (connect_data[i].error_counter >= connect_data[i].max_errors)
               {
                  new_bar_length = max_bar_length;
               }
               else
               {
                  new_bar_length = connect_data[i].error_counter * connect_data[i].scale;
                  if (new_bar_length > max_bar_length)
                  {
                     new_bar_length = max_bar_length;
                  }
               }
               if (connect_data[i].bar_length[ERROR_BAR_NO] != new_bar_length)
               {
                  connect_data[i].red_color_offset = new_bar_length * step_size;
                  connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].red_color_offset;

                  if ((i < location_where_changed) && (redraw_everything == NO))
                  {
                     if (x == -1)
                     {
                        locate_xy_column(connect_data[i].long_pos,
                                         &x, &y, &column);
                     }

                     if (connect_data[i].bar_length[ERROR_BAR_NO] < new_bar_length)
                     {
                        connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
                        draw_bar(i, 1, ERROR_BAR_NO, x, y + bar_thickness_2, column);
                     }
                     else
                     {
                        connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
                        draw_bar(i, -1, ERROR_BAR_NO, x, y + bar_thickness_2, column);
                     }
                     flush = YES;
                  }
               }
            }
         }
      }

      /* Redraw the line. */
      if ((i >= location_where_changed) && (redraw_everything == NO))
      {
#ifdef _DEBUG
         (void)fprintf(stderr, "count_channels: i = %d\n", i);
#endif
         flush = YES;
         draw_line_status(i, 1);
      }
   }
   if (redraw_everything == YES)
   {
      calc_but_coord(window_width);
      redraw_all();
      flush = YES;
   }

   /* Make sure all changes are shown. */
   if ((flush == YES) || (flush == YUP))
   {
      XFlush(display);

      if (flush != YUP)
      {
         redraw_time_host = MIN_REDRAW_TIME;
      }
   }
   else
   {
      if (redraw_time_host < MAX_REDRAW_TIME)
      {
         redraw_time_host += REDRAW_STEP_TIME;
      }
#ifdef _DEBUG
      (void)fprintf(stderr, "count_channels: Redraw time = %d\n", redraw_time_host);
#endif
   }

   /* Redraw every redraw_time_host ms. */
   (void)XtAppAddTimeOut(app, redraw_time_host,
                         (XtTimerCallbackProc)check_host_status, w);
 
   return;
}


/*+++++++++++++++++++++++++ calc_transfer_rate() ++++++++++++++++++++++++*/
static void
calc_transfer_rate(int i, clock_t end_time)
{
   register int  j;
   u_off_t       bytes_send = 0;

   for (j = 0; j < fsa[i].allowed_transfers; j++)
   {
      if (fsa[i].job_status[j].bytes_send != connect_data[i].bytes_send[j])
      {
         /* Check if an overrun has occurred. */
         if (fsa[i].job_status[j].bytes_send < connect_data[i].bytes_send[j])
         {
            connect_data[i].bytes_send[j] = 0;
         }

         bytes_send += (fsa[i].job_status[j].bytes_send -
                        connect_data[i].bytes_send[j]);

         connect_data[i].bytes_send[j] = fsa[i].job_status[j].bytes_send;
      }
   }

   if (bytes_send > 0)
   {
      clock_t delta_time;

      if ((delta_time = (end_time - connect_data[i].start_time)) == 0)
      {
         delta_time = 1;
      }
      else
      {
         if (delta_time > 1)
         {
            delta_time -= 1;
         }
      }
      connect_data[i].start_time = end_time;
      connect_data[i].bytes_per_sec = bytes_send * clktck / delta_time;

      /* Arithmetischer Mittelwert. */
      connect_data[i].average_tr = (double)(connect_data[i].average_tr +
                                   (double)connect_data[i].bytes_per_sec) /
                                   (double)2.0;

      if (connect_data[i].average_tr > connect_data[i].max_average_tr)
      {
         connect_data[i].max_average_tr = connect_data[i].average_tr;
      }
   }
   else
   {
      connect_data[i].bytes_per_sec = 0;

      if (connect_data[i].average_tr > (double)0.0)
      {
         /* Arithmetischer Mittelwert. */
         connect_data[i].average_tr = (double)(connect_data[i].average_tr +
                                      (double)connect_data[i].bytes_per_sec) /
                                      (double)2.0;

         if (connect_data[i].average_tr > connect_data[i].max_average_tr)
         {
            connect_data[i].max_average_tr = connect_data[i].average_tr;
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ check_fsa_data() ++++++++++++++++++++++++++*/
static int
check_fsa_data(unsigned int host_id)
{
   register int i;

   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].host_id == host_id)
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++ check_disp_data() ++++++++++++++++++++++++++*/
static int
check_disp_data(unsigned int host_id, int prev_no_of_hosts)
{
   register int i;

   for (i = 0; i < prev_no_of_hosts; i++)
   {
      if (connect_data[i].host_id == host_id)
      {
         return(i);
      }
   }

   return(INCORRECT);
}
