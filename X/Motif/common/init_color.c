/*
 *  init_color.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_color - initializes color for an X dialog
 **
 ** SYNOPSIS
 **   void init_color(Display *p_disp)
 **
 ** DESCRIPTION
 **   This function initializes the 'color_pool' array with color
 **   values according to the X Window System. If it fails to lookup
 **   a color it will try to use one of the reserve colors. For
 **   each color it is possible to specify three reserve colors.
 **   If all colors fail then it will either default to black or
 **   white.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.12.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <X11/Xlib.h>
#include "x_common_defs.h"

/* External global variables */
extern unsigned long color_pool[];
extern Colormap      default_cmap;


/*############################ init_color() #############################*/
void
init_color(Display *p_disp)
{
   char     *p_color[COLOR_POOL_SIZE][4] =
            {
               { DEFAULT_BG_COLOR, DEFAULT_BG_COLOR_1, DEFAULT_BG_COLOR_2, DEFAULT_BG_COLOR_3 },                                             /*                             */
                                                                                                                                             /* default background color    */
               { WHITE_COLOR, WHITE_COLOR_1, WHITE_COLOR_2, WHITE_COLOR_3 },                                                                 /*                             */
               { NOT_WORKING_COLOR, NOT_WORKING_COLOR_1, NOT_WORKING_COLOR_2, NOT_WORKING_COLOR_3 },                                         /* connection not working      */
               { NOT_WORKING2_COLOR, NOT_WORKING2_COLOR_1, NOT_WORKING2_COLOR_2, NOT_WORKING2_COLOR_3 },                                     /* connection not working      */
               { STOP_TRANSFER_COLOR, STOP_TRANSFER_COLOR_1, STOP_TRANSFER_COLOR_2, STOP_TRANSFER_COLOR_3 },                                 /* transfer stopped            */
               { TRANSFER_ACTIVE_COLOR, TRANSFER_ACTIVE_COLOR_1, TRANSFER_ACTIVE_COLOR_2, TRANSFER_ACTIVE_COLOR_3 } ,                        /* transfer active             */
               { PAUSE_QUEUE_COLOR, PAUSE_QUEUE_COLOR_1, PAUSE_QUEUE_COLOR_2, PAUSE_QUEUE_COLOR_3 },                                         /* stop generating messages    */
               { NORMAL_STATUS_COLOR, NORMAL_STATUS_COLOR_1, NORMAL_STATUS_COLOR_2, NORMAL_STATUS_COLOR_3 },                                 /* normal status               */
               { CONNECTING_COLOR, CONNECTING_COLOR_1, CONNECTING_COLOR_2, CONNECTING_COLOR_3 },                                             /* connecting                  */
               { BLACK_COLOR, BLACK_COLOR_1, BLACK_COLOR_2, BLACK_COLOR_3 },                                                                 /* foreground color            */
               { LOCKED_INVERSE_COLOR, LOCKED_INVERSE_COLOR_1, LOCKED_INVERSE_COLOR_2, LOCKED_INVERSE_COLOR_3 },                             /* inverse color when holding  */
                                                                                                                                             /* Ctrl Key.                   */
               { TR_BAR_COLOR, TR_BAR_COLOR_1, TR_BAR_COLOR_2, TR_BAR_COLOR_3 },                                                             /* color for transfer rate bar */
               { LABEL_BG_COLOR, LABEL_BG_COLOR_1, LABEL_BG_COLOR_2, LABEL_BG_COLOR_3 },                                                     /* background for label        */
               { FAULTY_TRANSFERS_COLOR, FAULTY_TRANSFERS_COLOR_1, FAULTY_TRANSFERS_COLOR_2, FAULTY_TRANSFERS_COLOR_3 },                     /* When logging in failed or   */
                                                                                                                                             /* we cannot access the        */
                                                                                                                                             /* directory where we have to  */
                                                                                                                                             /* store the files.            */
               { EMAIL_ACTIVE_COLOR, EMAIL_ACTIVE_COLOR_1, EMAIL_ACTIVE_COLOR_2, EMAIL_ACTIVE_COLOR_3 },                                     /* color to indicate that an   */
                                                                                                                                             /* email is being send         */
               { CHAR_BACKGROUND_COLOR, CHAR_BACKGROUND_COLOR_1, CHAR_BACKGROUND_COLOR_2, CHAR_BACKGROUND_COLOR_3 },                         /* color for background of     */
                                                                                                                                             /* characters                  */
               { FTP_BURST_TRANSFER_ACTIVE_COLOR, FTP_BURST_TRANSFER_ACTIVE_COLOR_1, FTP_BURST_TRANSFER_ACTIVE_COLOR_2, FTP_BURST_TRANSFER_ACTIVE_COLOR_3 }, /* When transmitting files     */
                                                                                                                                             /* without connecting.         */
#ifdef _WITH_WMO_SUPPORT
               { WMO_BURST_TRANSFER_ACTIVE_COLOR, WMO_BURST_TRANSFER_ACTIVE_COLOR_1, WMO_BURST_TRANSFER_ACTIVE_COLOR_2, WMO_BURST_TRANSFER_ACTIVE_COLOR_3 },
#endif
               { AUTO_PAUSE_QUEUE_COLOR, AUTO_PAUSE_QUEUE_COLOR_1, AUTO_PAUSE_QUEUE_COLOR_2, AUTO_PAUSE_QUEUE_COLOR_3 }                      /* automatic stop of generating*/
                                                                                                                                             /* messages                    */
            };
   int      i;
   XColor   dummy,
            color;

   /* Set up all colors */
   for (i = 0; i < COLOR_POOL_SIZE; i++)
   {
      if (XAllocNamedColor(p_disp, default_cmap, p_color[i][0], &dummy,
                           &color) == 0)
      {
         if (XAllocNamedColor(p_disp, default_cmap, p_color[i][1], &dummy,
                              &color) == 0)
         {
            if (XAllocNamedColor(p_disp, default_cmap, p_color[i][2], &dummy,
                                 &color) == 0)
            {
               if (XAllocNamedColor(p_disp, default_cmap, p_color[i][3], &dummy,
                                    &color) == 0)
               {
                  if (i == BLACK)
                  {
                     color_pool[i] = BlackPixel(p_disp, DefaultScreen(p_disp));
                  }
                  else
                  {
                     color_pool[i] = WhitePixel(p_disp, DefaultScreen(p_disp));
                  }
               }
               else
               {
                  color_pool[i] = color.pixel;
               }
            }
            else
            {
               color_pool[i] = color.pixel;
            }
         }
         else
         {
            color_pool[i] = color.pixel;
         }
      }
      else
      {
          color_pool[i] = color.pixel;
      }
   }

   return;
}