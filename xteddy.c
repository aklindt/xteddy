/* Xteddy - a cuddly bear to place on your desktop. */
/* Author: Stefan Gustavson, ISY-LiTH, 1994         */
/* Internet email address: stegu@itn.liu.se         */
/* This software is distributed under the GNU       */
/* Public Licence (GPL).                            */
/* Also, if you modify this program or include it   */
/* in some kind of official distribution, I would   */
/* like to know about it.                           */

/* Xpm pixmap manipulation routines for color       */
/* and grayscale teddies are from the Xpm library   */
/* by Arnaud Le Hors, lehors@sophia.inria.fr,       */
/* Copyright 1990-93 GROUPE BULL                    */

/* This is Xteddy version 2.2 as of 2009-02-25.     */
/* from  Peter De Wachter <pdewacht@gmail.com>      */
/* Changes: see ChangeLog                           */

#define DEFAULT_IMAGE_DIR "/usr/share/xteddy"
#define DEFAULT_LOCAL_IMAGE_DIR "/usr/local/share/xteddy"
#define XTEDDY_VERSION "2.02"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>

#include <Imlib2.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static char *progname;
Display *display;
int screen_num;

ino_t xteddy_inodenum;	/*  the inode of the executable. */
struct stat stat_buf_xteddy;

char *Extensions[] = {
  "xpm",
  "png",
  "tif",
  "jpg",
  "jpeg",
  "gif",
  "pnm",
  "XPM",
  "PNG",
  "TIF",
  "JPG",
  "JPEG",
  "GIF",
  "PNG",
  NULL
};

char *InitTeddy(char *teddy)
/* Initializing filename */
{
  char     fbuf[MAXPATHLEN], sbuf[MAXPATHLEN], *name, *ext, *sext;
  struct   stat stat_buf;
  char   **ptr;
  ino_t   teddy_inodenum;

  if ( !teddy || !*(teddy) ) return NULL;
  if ( !stat(teddy, &stat_buf) ){
    /* If the executable is in the current directory, xteddy will try to load itself (the binary!) and fail. Don't do this. */
    teddy_inodenum=stat_buf.st_ino;
    if (teddy_inodenum==xteddy_inodenum){
      fprintf(stderr, "Warning: the file named '%s' in the current directory is the executable program itself! Not trying to display it as an image.\n", teddy);
      /* Note, if the current dir contains a subdir named xteddy and xteddy is invoked as "xteddy" without the path, this error will also be triggered. The error message above
      will be (misleadingly) printed, but the correct action (not trying to load a directory as an image) will occur. */
    }else{
      return strdup(teddy);
    }
  }

  /* Test, some image formats PIXMAP_PATH */
  strcat(strcpy(fbuf, PIXMAP_PATH), "/");
  strcat(strcat(strcpy(sbuf, DEFAULT_IMAGE_DIR), "/"), teddy);
  name = fbuf + strlen(fbuf);
  strcpy(name, teddy);
  if ( !stat(fbuf, &stat_buf) ) return strdup(fbuf);
  if ( !stat(sbuf, &stat_buf) ) return strdup(sbuf);
  strcat(name, ".");
  ext  = name + strlen(name);
  sext = sbuf + strlen(sbuf);
  for ( ptr = Extensions; *ptr; ptr++ ) {
    strcat(name, *ptr);
    if ( !stat(name, &stat_buf) ) return strdup(name);
    if ( !stat(fbuf, &stat_buf) ) return strdup(fbuf);
    strcat(sbuf, ".");   /* missing the dot before the extension! */
    strcat(sbuf, *ptr);
    if ( !stat(sbuf, &stat_buf) ) return strdup(sbuf);
    *ext = *sext = 0;
  }

  /* Repeat with a different search path. */
  strcat(strcat(strcpy(sbuf, DEFAULT_LOCAL_IMAGE_DIR), "/"), teddy);
  if ( !stat(sbuf, &stat_buf) ) return strdup(sbuf);
  sext = sbuf + strlen(sbuf);
  for ( ptr = Extensions; *ptr; ptr++ ) {
    strcat(sbuf, ".");   /* missing the dot before the extension! */
    strcat(sbuf, *ptr);
    if ( !stat(sbuf, &stat_buf) ) return strdup(sbuf);
    *sext = 0;
  }

  /* failed to find anything. */
  return NULL;
}

int main(int argc, char **argv)
{
  /* Display, window and gc manipulation variables */
  Window               win;
  Pixmap               pm_image, pm_mask;
  XSetWindowAttributes setwinattr;
  unsigned long        valuemask, inputmask;
  int                  x, y, geomflags;
  unsigned int         xw, xh;
  unsigned int         border_width = 0;
  unsigned int         display_width, display_height;
  XSizeHints           size_hints;
  XWMHints             wm_hints;
  XClassHint           class_hints;
#ifdef FORTIFY_SOURCE
  XTextProperty        windowName, iconName;
#else
  XTextProperty        windowName;
#endif
  int                  argnum;
  int                  use_wm, float_up, allow_quit, cursor_blank;
  XEvent               report;
  char                *display_name = NULL;
  char                 buffer[20];
  int                  bufsize = 20;
  KeySym               keysym;
  XComposeStatus       compose;
  int                  charcount;
  Cursor               cursor;
  char                *teddy;
  char                *file;

  /* Window movement variables */
  XWindowChanges       winchanges;
  Window               root, child, basewin;
  int                  offs_x, offs_y, new_x, new_y, tmp_x, tmp_y;
  unsigned int         tmp_mask;

  Imlib_Image         *im;
  int                  im_width, im_height;

  /* Determine program name */
  if ((progname = strrchr(argv[0],'/')) == NULL)
    progname = argv[0];
  else
    progname++;
  teddy = progname;


  /* Option handling: "-wm", "-float", "-noquit", "-geometry", */
  /* "-display" and "-nocursor" are recognized. See manual page for details. */
  /* -F<name> ... Other pixmap name */
  /* -F or -f are equivalent. A space may be included if desired after the option (eg "-f xduck") */
  /* -v prints version. -h or --help prints help */
  use_wm = FALSE;
  float_up = FALSE;
  allow_quit = TRUE;
  cursor_blank = FALSE;
  x = y = 0;
  geomflags = 0;
  for ( argnum = 1; argnum < argc; argnum++ ) {
    if (!strcmp(argv[argnum],"-wm"))
      use_wm = TRUE;
    if (!strcmp(argv[argnum],"-float"))
      float_up = TRUE;
    if (!strcmp(argv[argnum],"-noquit"))
      allow_quit = FALSE;
    if (!strcmp(argv[argnum],"-geometry"))
      geomflags = XParseGeometry(argv[++argnum], &x, &y, &xw, &xh);
    if (!strcmp(argv[argnum],"-display"))
      display_name = argv[++argnum];
    if (!strcmp(argv[argnum],"-nocursor"))
      cursor_blank = TRUE;
    /* Use -f or -F; -f seems more natural. But -float does not mean "-f loat" ! */
    if ( (!strncmp(argv[argnum],"-F", 2)) || ( (!strncmp(argv[argnum],"-f", 2)) && (strcmp(argv[argnum],"-float")) ) )
      teddy = argv[argnum] + 2;
      if (strlen(teddy) == 0){	/* accept "-f <name>" as well as "-f<name>" */
        teddy = argv[argnum] + 3;
        argnum ++;
      }
    if (!strcmp(argv[argnum],"-v")){
     fprintf(stderr,"xteddy version: %s\n",XTEDDY_VERSION);
     exit (0);
    }
    if ((!strncmp(argv[argnum],"-h", 2)) || (!strncmp(argv[argnum],"--help", 2))){  /* show help */
      fprintf (stderr,"xteddy is a cute teddy bear that sits on your X desktop.\n");
      fprintf (stderr,"Usage: xteddy [ OPTIONS ] [ -f <filename> ]\n");
      fprintf (stderr,"        -wm                     use the window manager\n");
      fprintf (stderr,"        -float                  always float on top of other windows\n");
      fprintf (stderr,"        -noquit                 disable the 'q' key to quit.\n");
      fprintf (stderr,"        -f,-F <filename>        use <filename> as the image.\n");
      fprintf (stderr,"        -geometry,-display      have their usual meaning in X.\n");
      fprintf (stderr,"        -h, --help              display this help and exit\n");
      fprintf (stderr,"        -v                      print version information and exit\n\n");
      fprintf (stderr,"The default image is a teddy bear, but others may be selected with -f (or -F).\n");
      fprintf (stderr,"To tuck him back up in bed, move the mouse over him and press the 'q' or 'Esc' key.\n");
      fprintf (stderr,"This is xteddy version %s. See `man 1 xteddy` for more information.\n",XTEDDY_VERSION);
      exit (0);
    }
  }

  stat(argv[0], &stat_buf_xteddy);  /* find the inode number of the executable file itself */
  xteddy_inodenum=stat_buf_xteddy.st_ino; /* [This can be fooled if it is invoked as "xteddy" without the path, AND eg the working directory contains a subdirectory called xteddy] */

  /* Connect to X server */
  if ( (display = XOpenDisplay(display_name)) == NULL )
    {
      (void) fprintf(stderr, "%s: Cannot connect to X server %s\n",
		     progname, XDisplayName(display_name));
      exit(-1);
    }
  /* Get screen size and depth */
  display_width  = DisplayWidth(display, screen_num);
  display_height = DisplayHeight(display, screen_num);

  /* set the maximum number of colors to allocate for 8bpp and less to 128 */  
  imlib_set_color_usage(128);
  /* dither for depths < 24bpp */
  imlib_context_set_dither(1);
  /* set the display, visual and colormap we are using */  
  imlib_context_set_display(display);
  imlib_context_set_visual(DefaultVisual(display, DefaultScreen(display)));
  imlib_context_set_colormap(DefaultColormap(display, DefaultScreen(display)));

  if ( !(file = InitTeddy(teddy)) ) {
    fprintf(stderr, "Can not find any image with name '%s'.\n", teddy);
    return -1;
  }
  if ( !(im=imlib_load_image_immediately(file)) ) {
    fprintf(stderr, "Most probably, the file '%s' is not a valid image.\n", teddy);
    return -1;
  }
  imlib_context_set_image(im);
  im_width = imlib_image_get_width();
  im_height = imlib_image_get_height();
  /* Set the window position according to user preferences */
  if (geomflags & XNegative)
    x = display_width - im_width + x;
  if (geomflags & YNegative)
    y = display_height - im_height + y;
  /* Clip against bounds to stay on the screen */
  if (x<0) x=0;
  if (x > display_width - im_width) x = display_width - im_width;
  if (y<0) y=0;
  if (y > display_height - im_height) y = display_height - im_height;

  /* Create the main window */
  win = XCreateSimpleWindow(display, DefaultRootWindow(display),
			    x,y,im_width,im_height,border_width,
			    BlackPixel(display,screen_num),
			    WhitePixel(display,screen_num));
  XSelectInput(display,win,StructureNotifyMask);

  imlib_context_set_drawable(win);
  imlib_render_pixmaps_for_whole_image(&pm_image, &pm_mask);
  XSetWindowBackgroundPixmap(display, win, pm_image);
  XShapeCombineMask(display, win, ShapeBounding, 0, 0, pm_mask, ShapeSet);
  XFreePixmap(display, pm_image);
  XFreePixmap(display, pm_mask);
  imlib_free_image_and_decache();

  basewin = win;

  if (use_wm)
    setwinattr.override_redirect = FALSE;
  else
    setwinattr.override_redirect = TRUE;

  if (cursor_blank)
    {
      Pixmap blank;
      XColor dummy;
      const char data[1] = { 0 };
      blank = XCreateBitmapFromData (display, win, data, 1, 1);
      if (blank == None)
        {
          fprintf (stderr, "error: out of memory.\n");
          exit (1);
        }
      cursor = XCreatePixmapCursor (display, blank, blank, &dummy, &dummy, 0, 0);
      XFreePixmap (display , blank);
    }
  else
    cursor = XCreateFontCursor(display, XC_heart);

  setwinattr.cursor = cursor;
  valuemask = CWOverrideRedirect | CWCursor;
  XChangeWindowAttributes(display, win, valuemask, &setwinattr);

  /* Report size hints and other stuff to the window manager */
  size_hints.min_width  = im_width;    /* Don't allow any resizing */
  size_hints.min_height = im_height;
  size_hints.max_width  = im_width;
  size_hints.max_height = im_height;
  size_hints.flags = PPosition | PSize | PMinSize | PMaxSize;
  if (XStringListToTextProperty(&(teddy), 1, &windowName) == 0)
    {
      (void) fprintf(stderr, "%s: structure allocation for windowName failed.\n", progname);
      return -1;
    }
  wm_hints.initial_state = NormalState;
  wm_hints.input = TRUE;
  wm_hints.flags = StateHint | IconPixmapHint | InputHint;

  class_hints.res_name = progname;
  class_hints.res_class = "Xteddy";

#ifdef FORTIFY_SOURCE
  XSetWMProperties(display, win, &windowName, &iconName,
		   argv, argc, &size_hints, &wm_hints, &class_hints);
#else
  XSetWMProperties(display, win, &windowName, NULL,
		   argv, argc, &size_hints, &wm_hints, &class_hints);
#endif

  /* Select event types wanted */
  inputmask = ExposureMask | KeyPressMask | ButtonPressMask |
    ButtonReleaseMask | StructureNotifyMask | ButtonMotionMask |
      PointerMotionHintMask | EnterWindowMask | LeaveWindowMask;
  if (float_up) inputmask |= VisibilityChangeMask;
  XSelectInput(display, win, inputmask);

  /* Display window */
  XMapWindow(display,win);

  /* Get and process the events */
  while (1)
    {
      XNextEvent(display, &report);
      switch(report.type)
	{
	case ReparentNotify:
	  /* Window was reparented by the window manager */
	  if (!use_wm)
	    (void) fprintf(stderr,
			   "%s: Window manager wouldn't leave the window alone!\n",
			   progname);
	  basewin = report.xreparent.parent;
	  break;
	case EnterNotify:
	  /* Grab the keyboard while the pointer is in the window */
	  XGrabKeyboard(display, win, FALSE, GrabModeAsync, GrabModeAsync,
			CurrentTime);
	  break;
	case LeaveNotify:
	  /* Release the keyboard when the pointer leaves the window */
	  XUngrabKeyboard(display, CurrentTime);
	  break;
	case ButtonPress:
	  /* Raise xteddy above sibling windows  */
	  XRaiseWindow(display, win);
	  /* Remember where the mouse went down */
	  XQueryPointer(display, basewin, &root, &child, &tmp_x, &tmp_y,
			&offs_x, &offs_y, &tmp_mask);
	  break;
	case ButtonRelease:
	  /* Place xteddy at the new position */
	  XQueryPointer(display, basewin, &root, &child, &new_x, &new_y,
		        &tmp_x, &tmp_y, &tmp_mask);
	  winchanges.x = new_x - offs_x;
	  winchanges.y = new_y - offs_y;
	  XReconfigureWMWindow(display, basewin, screen_num,
			       CWX | CWY, &winchanges);
	  break;
	case MotionNotify:
	  /* Move xteddy around with the mouse */
	  while (XCheckMaskEvent(display, ButtonMotionMask, &report));
	  if (!XQueryPointer(display, report.xmotion.window, &root, &child,
			    &new_x, &new_y, &tmp_x, &tmp_y, &tmp_mask))
	    break;
	  winchanges.x = new_x - offs_x;
	  winchanges.y = new_y - offs_y;
	  XReconfigureWMWindow(display, win, screen_num,
			       CWX | CWY, &winchanges);
	  break;
	case VisibilityNotify:
	  /* Put xteddy on top of overlapping windows */
	  if (float_up)
	    if ((report.xvisibility.state == VisibilityFullyObscured)
		|| (report.xvisibility.state == VisibilityPartiallyObscured))
	      XRaiseWindow(display,win);
	  break;
	case KeyPress:
	  /* Exit on "q" or "Q" or "Esc" */
	  charcount = XLookupString(&report.xkey, buffer, bufsize,
				    &keysym, &compose);
	  if((keysym == XK_Q) || (keysym == XK_q)  || (keysym == XK_Escape))
	    {
              if (allow_quit)
		{
		  XCloseDisplay(display);
		  return 0;
		}
	    }
	  break;
	default:
	  /* Throw away all other events */
	  break;
	} /* end switch */
    } /* end while */
}

