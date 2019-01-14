/*
 * tkvlc.c
 *
 * A demo to embed libVLC to a Tk toolkit frame widget
 * Copyright (C) Danilo Chang 2017
 * Copyright (c) Christian Werner 2019
 */

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_TK_PHOTO
#include <tk.h>
#else
#include <tcl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vlc/vlc.h>
#include <vlc/libvlc_version.h>

#ifdef _WIN32
#include <windows.h>
#endif

/*
 * Only the _Init function is exported.
 */

extern DLLEXPORT int	Tkvlc_Init(Tcl_Interp * interp);

/*
 * end block for C++
 */

#ifdef __cplusplus
}
#endif

#ifdef USE_TK_PHOTO

typedef struct {
  struct libVLCData *p;
  int busy;
  unsigned char *pixels;
} libVLCFrame;

typedef struct {
  Tcl_Event header;
  libVLCFrame *f;
} libVLCEvent;


#else

#ifdef _WIN32
HWND hwnd;
#else
uint32_t drawable;
#endif

#endif

typedef struct libVLCData {
  libvlc_instance_t *vlc_inst;
  Tcl_Interp *interp;
  Tcl_Command cmd;
  libvlc_media_player_t *media_player;
  libvlc_media_t *media;
#ifdef USE_TK_PHOTO
  int tk_checked;
  Tcl_Obj *photo_name;
  int width, height;
  Tcl_ThreadId tid;
  libVLCFrame frames[2];
#endif
} libVLCData;


#ifdef USE_TK_PHOTO

/*
 *----------------------------------------------------------------------
 *
 * silence --
 *
 *	Dummy procedure to silence libvlc log messages.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void silence(void *data, int level, const libvlc_log_t *ctx,
		    const char *fmt, va_list args)
{
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_check --
 *
 *	Lazy load Tk and check for its availability.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tk package may be loaded.
 *
 *----------------------------------------------------------------------
 */

static int Tk_check(libVLCData *p, Tcl_Interp *interp)
{
  if (p->tk_checked > 0) {
    return TCL_OK;
  } else if (p->tk_checked < 0) {
    Tcl_SetResult(interp, "can't find package Tk", TCL_STATIC);
    return TCL_ERROR;
  }
#ifdef USE_TK_STUBS
  if (Tk_InitStubs(interp, "8.5", 0) == NULL) {
    p->tk_checked = -1;
    return TCL_ERROR;
  }
#else
  if (Tcl_PkgRequire(interp, "Tk", "8.5", 0) == NULL) {
    p->tk_checked = -1;
    return TCL_ERROR;
  }
#endif
  p->tk_checked = 1;
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * libVLCready --
 *
 *	Procedure called by the Tcl event mechanism. The event
 *	indicates a filled video buffer which is copied to the
 *	photo image.
 *
 * Results:
 *	Always true (event handled).
 *
 * Side effects:
 *	Playback is stopped when the photo image is invalid.
 *
 *----------------------------------------------------------------------
 */

static int libVLCready(Tcl_Event *ev, int flags)
{
  libVLCEvent *e = (libVLCEvent *) ev;
  libVLCFrame *f = e->f;
  libVLCData *p = f->p;
  Tcl_Interp *interp = p->interp;
  Tk_PhotoHandle photo;

  if (f->busy < 0) {
    /* about to tear down media player */
    goto done;
  }
  photo = Tk_FindPhoto(interp, Tcl_GetString(p->photo_name));
  if (photo == NULL) {
    libvlc_media_player_stop(p->media_player);
  } else {
    Tk_PhotoImageBlock blk;

    blk.width = p->width;
    blk.height = p->height;
    blk.pixelSize = 3;
    blk.pitch = blk.width * blk.pixelSize;
    blk.pixelPtr = f->pixels;
    blk.offset[0] = 0;
    blk.offset[1] = 1;
    blk.offset[2] = 2;
    blk.offset[3] = 3;
    if (Tk_PhotoExpand(interp, photo, blk.width, blk.height) == TCL_OK) {
      Tk_PhotoPutBlock(interp, photo, &blk, 0, 0, blk.width, blk.height,
		       TK_PHOTO_COMPOSITE_SET);
    }
  }
  Tcl_ResetResult(interp);
done:
  f->busy = 0;
  return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * libVLClock --
 *
 *	Procedure called by libvlc when a new video frame is
 *	to be decoded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void *libVLClock(void *clientData, void **planes)
{
  libVLCData *p = (libVLCData *) clientData;
  int index = 0;

  if (!p->frames[index].busy) {
    ++index;
  }
  planes[0] = p->frames[index].pixels;
  return &p->frames[index];
}

/*
 *----------------------------------------------------------------------
 *
 * libVLCdisplay --
 *
 *	Procedure called by libvlc when a new video frame is
 *	to be displayed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl event is queued and the thread owning the
 *	media player is alerted.
 *
 *----------------------------------------------------------------------
 */

static void libVLCdisplay(void *clientData, void *picture)
{
  libVLCData *p = (libVLCData *) clientData;
  libVLCFrame *f = (libVLCFrame *) picture;
  libVLCEvent *e;

  if ((f == &p->frames[0] && p->frames[1].busy) ||
      (f == &p->frames[1] && p->frames[0].busy)) {
    /* other buffer still in use, drop frame */
    return;
  }
  f->busy = 1;
  e = (libVLCEvent *) ckalloc(sizeof(*e));
  e->header.proc = libVLCready;
  e->header.nextPtr = NULL;
  e->f = f;
  Tcl_ThreadQueueEvent(p->tid, &e->header, TCL_QUEUE_TAIL);
  Tcl_ThreadAlert(p->tid);
}

#endif


/*
 *----------------------------------------------------------------------
 *
 * libVLCObjCmd --
 *
 *	Tcl command to deal with the libvlc media player.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Many depending on command arguments.
 *
 *----------------------------------------------------------------------
 */

int libVLCObjCmd(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  libVLCData *pVLC = (libVLCData *) cd;
  int choice;
  int rc = TCL_OK;

  static const char *VLC_strs[] = {
    "open", "play", "pause", "stop", "isplaying",
    "volume", "duration", "time", "position",
    "rate", "isseekable", "state", "version", "destroy",
    NULL
  };
  enum VLC_enum {
    TKVLC_OPEN, TKVLC_PLAY, TKVLC_PAUSE, TKVLC_STOP, TKVLC_ISPLAYING,
    TKVLC_VOLUME, TKVLC_DURATION, TKVLC_TIME, TKVLC_POSITION,
    TKVLC_RATE, TKVLC_ISSEEKABLE, TKVLC_STATE, TKVLC_VERSION, TKVLC_DESTROY
  };

  if( objc < 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return TCL_ERROR;
  }

  if( Tcl_GetIndexFromObj(interp, objv[1], VLC_strs, "option", 0, &choice) ){
    return TCL_ERROR;
  }

  switch( (enum VLC_enum)choice ){

    case TKVLC_OPEN: {
        char *filename = NULL;
        int len = 0;
        int status = 0;

        if( objc != 3 ){
            Tcl_WrongNumArgs(interp, 2, objv, "filename");
            return TCL_ERROR;
        }

        filename = Tcl_GetStringFromObj(objv[2], &len);
        if( !filename || len < 1 ) {
            return TCL_ERROR;
        }

        pVLC->media = libvlc_media_new_path(pVLC->vlc_inst, filename);
        if(pVLC->media == NULL) {  // Is it necessary?
            Tcl_AppendResult(interp, "libvlc_media_new_path failed.", (char*)0);
            return TCL_ERROR;
        }

        libvlc_media_player_set_media(pVLC->media_player, pVLC->media);
#if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
        status = libvlc_media_parse_with_options(pVLC->media, libvlc_media_parse_local, -1);
        if (status < 0) {
            Tcl_AppendResult(interp, "libvlc_media_parse_with_options failed.", (char*)0);
            return TCL_ERROR;
        }
#else
        libvlc_media_parse(pVLC->media);
#endif

        libvlc_media_player_play(pVLC->media_player); // Play media
        libvlc_media_release(pVLC->media);

        break;
    }

    case TKVLC_PLAY: {
        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if(libvlc_media_player_is_playing(pVLC->media_player) == 0) {
            libvlc_media_player_play(pVLC->media_player);
        }

        break;
    }

    case TKVLC_PAUSE: {
        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if(libvlc_media_player_is_playing(pVLC->media_player) == 1) {
            libvlc_media_player_pause(pVLC->media_player);
        }

        break;
    }

    case TKVLC_STOP: {
        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        libvlc_media_player_stop(pVLC->media_player);

        break;
    }

    case TKVLC_ISPLAYING: {
        Tcl_Obj *return_obj;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if(libvlc_media_player_is_playing(pVLC->media_player) == 1) {
            return_obj = Tcl_NewBooleanObj(1);
        } else {
            return_obj = Tcl_NewBooleanObj(0);
        }

        Tcl_SetObjResult(interp, return_obj);

        break;
    }

    case TKVLC_VOLUME: {
      int volume;

      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?value?");
        return TCL_ERROR;
      }
      if (objc > 2) {
        if (Tcl_GetIntFromObj(interp, objv[2], &volume) != TCL_OK) {
          return TCL_ERROR;
        }
        /* 0 if the volume was set, -1 if it was out of range */
        if (libvlc_audio_set_volume(pVLC->media_player, volume) == 0) {
          Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
        } else {
          Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
        }
      } else {
        volume = libvlc_audio_get_volume (pVLC->media_player);
        Tcl_SetObjResult(interp, Tcl_NewIntObj(volume));
      }
      break;
    }

    case TKVLC_DURATION: {
        Tcl_Obj *return_obj;
        libvlc_time_t tm;
        double result;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if(!pVLC->media) {
            return TCL_ERROR;
        }

        tm = libvlc_media_get_duration(pVLC->media);
        result = (double) tm / 1000.0;
        return_obj = Tcl_NewDoubleObj(result);

        Tcl_SetObjResult(interp, return_obj);

        break;
    }

    case TKVLC_TIME: {
      libvlc_time_t tm;
      double t;

      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?value?");
        return TCL_ERROR;
      }
      if (objc > 2) {
        if (Tcl_GetDoubleFromObj(interp, objv[2], &t) != TCL_OK) {
          return TCL_ERROR;
        }
        /* set the movie time in ms, user should give the movie time in sec */
        tm = (libvlc_time_t) t * 1000;
        /* not all formats and protocols support this */
        libvlc_media_player_set_time(pVLC->media_player, tm);
      } else {
        tm = libvlc_media_player_get_time(pVLC->media_player);
        t = (double) tm / 1000.0;
        Tcl_SetObjResult(interp, Tcl_NewDoubleObj(t));
      }
      break;
    }

    case TKVLC_POSITION: {
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?value?");
        return TCL_ERROR;
      }
      if (objc > 2) {
        double pos;

        if (Tcl_GetDoubleFromObj(interp, objv[2], &pos) != TCL_OK) {
          return TCL_ERROR;
        }
        if (pos < 0.0 || pos > 1.0) {
          Tcl_SetResult(interp, "position must be between 0 and 1",
                        TCL_STATIC);
          return TCL_ERROR;
        }
        /*
         * Set movie position as percentage between 0.0 and 1.0
         * This has no effect if playback is not enabled.
         * This might not work depending on the underlying input
         * format and protocol.
         */
        libvlc_media_player_set_position(pVLC->media_player, (float) pos);
      } else {
        float pos = libvlc_media_player_get_position(pVLC->media_player);

        Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double) pos));
      }
      break;
    }

    case TKVLC_RATE: {
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?value?");
        return TCL_ERROR;
      }
      if (objc > 2) {
        double r;

        if (Tcl_GetDoubleFromObj(interp, objv[2], &r) != TCL_OK) {
          return TCL_ERROR;
        }
        /* not all formats and protocols support this */
        libvlc_media_player_set_rate(pVLC->media_player, (float) r);
      } else {
        float r = libvlc_media_player_get_rate(pVLC->media_player);

        Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double) r));
      }
      break;
    }

    case TKVLC_ISSEEKABLE: {
        Tcl_Obj *return_obj;
        int result;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        // true if the media player can seek
        result = libvlc_media_player_is_seekable(pVLC->media_player);
        if(result > 0) {
            return_obj = Tcl_NewBooleanObj(1);
        } else {
            return_obj = Tcl_NewBooleanObj(0);
        }

        Tcl_SetObjResult(interp, return_obj);

        break;
    }

    case TKVLC_STATE: {
        Tcl_Obj *return_obj = NULL;
        libvlc_state_t state;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        // get the current state of the media player (playing, paused, ...)
        state = libvlc_media_player_get_state(pVLC->media_player);

        if(state==libvlc_NothingSpecial) {
             return_obj = Tcl_NewStringObj("idle", -1);
        } else if(state==libvlc_Opening) {
             return_obj = Tcl_NewStringObj("opening", -1);
        } else if(state==libvlc_Buffering) {
             return_obj = Tcl_NewStringObj("buffering", -1);
        } else if(state==libvlc_Playing) {
             return_obj = Tcl_NewStringObj("playing", -1);
        } else if(state==libvlc_Paused) {
             return_obj = Tcl_NewStringObj("paused", -1);
        } else if(state==libvlc_Stopped) {
             return_obj = Tcl_NewStringObj("stopped", -1);
        } else if(state==libvlc_Ended) {
             return_obj = Tcl_NewStringObj("ended", -1);
        } else if(state==libvlc_Error) {
             return_obj = Tcl_NewStringObj("error", -1);
        }

        Tcl_SetObjResult(interp, return_obj);

        break;
    }

    case TKVLC_VERSION: {
      Tcl_Obj *return_obj;
      const char *result;

      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      result = libvlc_get_version();
      return_obj = Tcl_NewStringObj(result, -1);

      Tcl_SetObjResult(interp, return_obj);

      break;
    }

    case TKVLC_DESTROY: {
      if( objc != 2 ){
        Tcl_WrongNumArgs(interp, 2, objv, 0);
        return TCL_ERROR;
      }

      Tcl_DeleteCommandFromToken(interp, pVLC->cmd);
      break;
    }

  } /* End of the SWITCH statement */

  return rc;
}


/*
 *----------------------------------------------------------------------
 *
 * libVLCObjCmdDeleted --
 *
 *	Destructor of libvlc media player object and command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources are released.
 *
 *----------------------------------------------------------------------
 */

static void libVLCObjCmdDeleted(ClientData clientData)
{
  libVLCData *p = (libVLCData *) clientData;

  if (p->media_player != NULL) {
    libvlc_media_player_stop(p->media_player);
    libvlc_media_player_release(p->media_player);
  }
  libvlc_release(p->vlc_inst);

#ifdef USE_TK_PHOTO
  if (p->photo_name != NULL) {
    Tcl_DecrRefCount(p->photo_name);
  }
  if (p->frames[0].busy) {
    p->frames[0].busy = -1;
  }
  while (p->frames[0].busy) {
    if (Tcl_DoOneEvent(TCL_ALL_EVENTS) == 0) {
      break;
    }
  }
  if (p->frames[0].pixels != NULL) {
    ckfree(p->frames[0].pixels);
  }
  if (p->frames[1].busy) {
    p->frames[1].busy = -1;
  }
  while (p->frames[1].busy) {
    if (Tcl_DoOneEvent(TCL_ALL_EVENTS) == 0) {
      break;
    }
  }
  if (p->frames[1].pixels != NULL) {
    ckfree(p->frames[1].pixels);
  }
#endif

  ckfree((char *) p);
}

/*
 *----------------------------------------------------------------------
 *
 * TKVLC_INIT --
 *
 *	Initialize a libvlc media player given window identifier
 *	or photo image name.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory and resources are allocated, a Tcl command is
 *	created to refer to the media player.
 *
 *----------------------------------------------------------------------
 */

static int TKVLC_INIT(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
    const char *zArg;
    libVLCData *p;
#if !defined(_WIN32)
#if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
    const char *argv[] ={"--no-xlib"};
#endif
#endif

    if( objc != 2 && objc != 3 ) {
#ifdef USE_TK_PHOTO
      Tcl_WrongNumArgs(interp, 1, objv, "HANDLE ?photo?");
#else
      Tcl_WrongNumArgs(interp, 1, objv, "HANDLE ?HWND?");
#endif
      return TCL_ERROR;
    }

    p = (libVLCData *)Tcl_Alloc( sizeof(*p) );
    if( p==0 ) {
      Tcl_SetResult(interp, (char *)"malloc failed", TCL_STATIC);
      return TCL_ERROR;
    }

    memset(p, 0, sizeof(*p));
    p->interp = interp;
    p->vlc_inst = NULL;
    p->media_player = NULL;
    p->media = NULL;

#ifdef USE_TK_PHOTO
    p->tk_checked = 0;
    p->photo_name = NULL;
    p->width = p->height = 0;
    p->tid = Tcl_GetCurrentThread();
    memset(&p->frames, 0, sizeof(p->frames));
    p->frames[0].p = p->frames[1].p = p;
#endif

#ifdef _WIN32
    p->vlc_inst = libvlc_new(0, NULL);
#else
#if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
    p->vlc_inst = libvlc_new(1, argv);
#else
    p->vlc_inst = libvlc_new(0, NULL);
#endif
#endif
    if (p->vlc_inst == NULL) {
      Tcl_SetResult(interp, "vlc setup failed", TCL_STATIC);
      ckfree((char *) p);
      return TCL_ERROR;
    }

#ifdef USE_TK_PHOTO
    libvlc_log_set(p->vlc_inst, silence, NULL);
#endif

    p->media_player = libvlc_media_player_new(p->vlc_inst);
    if (p->media_player == NULL) {
      Tcl_SetResult(interp, "media player setup failed", TCL_STATIC);
      libvlc_release(p->vlc_inst);
      ckfree((char *) p);
      return TCL_ERROR;
    }

    /*
     * Tcl side use "winfo id window" to give a low-level
     * platform-specific identifier for window.
     *
     * On Unix platforms, this is the X window identifier.
     * Under Windows, this is the Windows HWND.
     */
    if (objc == 3) {
      int is_win = 0;

#ifdef USE_TK_PHOTO

    if (!is_win) {
      Tk_PhotoHandle photo;

      if (Tk_check(p, interp) != TCL_OK) {
	libvlc_media_player_release(p->media_player);
	libvlc_release(p->vlc_inst);
	ckfree((char *) p);
	return TCL_ERROR;
      }
      if (Tk_MainWindow(interp) == NULL) {
	Tcl_SetResult(interp, "application has been destroyed", TCL_STATIC);
	libvlc_media_player_release(p->media_player);
	libvlc_release(p->vlc_inst);
	ckfree((char *) p);
	return TCL_ERROR;
      }
      photo = Tk_FindPhoto(interp, Tcl_GetString(objv[2]));
      if (photo == NULL) {
	Tcl_SetResult(interp, "no valid photo image given", TCL_STATIC);
	libvlc_media_player_release(p->media_player);
	libvlc_release(p->vlc_inst);
	ckfree((char *) p);
	return TCL_ERROR;
      }
      Tk_PhotoGetSize(photo, &p->width, &p->height);
      if (p->width <= 0 || p->height <= 0) {
	p->width = 640;
	p->height = 480;
      }
      p->photo_name = objv[2];
      Tcl_IncrRefCount(p->photo_name);
      libvlc_video_set_callbacks(p->media_player, libVLClock, NULL,
				 libVLCdisplay, p);
      libvlc_video_set_format(p->media_player, "RV24", p->width, p->height,
			      p->width * 3);
      p->frames[0].pixels = ckalloc(p->width * p->height * 3);
      p->frames[1].pixels = ckalloc(p->width * p->height * 3);
    }

#else

#ifdef _WIN32
    Tcl_GetIntFromObj(interp, objv[2], (int*)&hwnd);
    libvlc_media_player_set_hwnd(p->media_player, (void *) hwnd);
    is_win = 1;
#else
#ifdef __APPLE__
    /* TBD */
#else
    Tcl_GetIntFromObj(interp, objv[2], (int *) &drawable);
    libvlc_media_player_set_xwindow(p->media_player, (uint32_t) drawable);
    is_win = 1;
#endif
#endif

#endif
    }

    zArg = Tcl_GetStringFromObj(objv[1], 0);
    p->cmd = Tcl_CreateObjCommand(interp, zArg, libVLCObjCmd, (char*)p, 
                        (Tcl_CmdDeleteProc *) libVLCObjCmdDeleted);

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Tkvlc_Init --
 *
 *	Initialize the new package.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side effects:
 *	The tkvlc package is created.
 *
 *----------------------------------------------------------------------
 */

int Tkvlc_Init(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8.5", 0) == NULL) {
    return TCL_ERROR;
  }
#else
  if (Tcl_PkgRequire(interp, "Tcl", "8.5", 0) == NULL) {
    return TCL_ERROR;
  }
#endif

    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "tkvlc::init", (Tcl_ObjCmdProc *) TKVLC_INIT,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
