/*
 * tkvlc.c
 *
 * A demo to embed libVLC to a Tk toolkit frame widget
 * Copyright (C) Danilo Chang 2017, 2020
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

extern DLLEXPORT int Tkvlc_Init(Tcl_Interp * interp);

/*
 * end block for C++
 */

#ifdef __cplusplus
}
#endif

#ifdef USE_TK_PHOTO

/*
 * Frame buffer when rendering to a photo image.
 */

typedef struct {
  struct libVLCData *p;     /* libvlc/Tcl instance data. */
  struct libVLCEvent *e;    /* Queued frame event or NULL. */
  int busy;                 /* True during rendering and event being queued. */
  unsigned char *pixels;    /* RGB frame buffer, size is width*height*3. */
} libVLCFrame;

/*
 * Event types for event callback
 */

#define EV_MEDIA_CHANGED 0              /* "media" */
#define EV_STATE_CHANGED 1              /* "state" */
#define EV_TIME_CHANGED  2              /* "time" */
#define EV_POS_CHANGED   3              /* "position" */
#define EV_AUDIO_CHANGED 4              /* "audio" */
#define EV_NEW_FRAME     5              /* "frame" */

/*
 * Event type for Tcl_QueueEvent()
 */

typedef struct libVLCEvent {
  Tcl_Event header;             /* Mandatory header. */
  int type;                     /* See EV_* defines above. */
  libVLCFrame *f;               /* Pointer to a frame buffer or NULL. */
  struct libVLCData *p;         /* Pointer to libvlc instance data. */
  struct libVLCEvent *next;     /* Linkage, unused for frame events. */
} libVLCEvent;

#else

#ifdef _WIN32
HWND hwnd;
#else
uint32_t drawable;
#endif

#endif

/*
 * libvlc instance data as used as ClientData of the Tcl command.
 */

typedef struct libVLCData {
  libvlc_instance_t *vlc_inst;          /* libvlc library instance data. */
  Tcl_Interp *interp;                   /* Associated Tcl interpreter. */
  Tcl_Command cmd;                      /* Tcl command token. */
  libvlc_media_player_t *media_player;  /* libvlc media player. */
#ifdef USE_TK_PHOTO
  int repeat;                           /* If true, replay media. */
  int tk_checked;                       /* True when Tk available. */
  Tcl_WideInt window_id;                /* Platform handle, if photo unused. */
#endif
  Tcl_Obj *file_name;                   /* Filename of last opened media. */
  int is_location;                      /* Indicate to use location api */
#ifdef USE_TK_PHOTO
  Tcl_Obj *photo_name;                  /* Name of photo image or NULL. */
  int width, height;                    /* Width and height for photo image. */
  Tcl_ThreadId tid;                     /* Thread identifier of interpreter. */
  Tcl_Mutex ev_lock;                    /* Mutex for ev_queue. */
  libVLCEvent *ev_queue;                /* Queued events minus frame events. */
  int nCmdObjs;                         /* Event callback information. */
  Tcl_Obj **cmdObjs;                    /* Ditto. */
  int nSavedCmdObjs;                    /* Ditto. */
  Tcl_Obj **savedCmdObjs;               /* Ditto. */
  libVLCFrame frames[2];                /* Frame buffers for photo images. */
#endif
} libVLCData;


#ifdef USE_TK_PHOTO

/*
 *----------------------------------------------------------------------
 *
 * silence --
 *
 *      Dummy procedure to silence libvlc log messages.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void silence(void *data, int level, const libvlc_log_t *ctx,
            const char *fmt, va_list args)
{
}

#ifdef linux
/*
 *----------------------------------------------------------------------
 *
 * Raspi_complain --
 *
 *      Half baked check for running on RaspberryPi.
 *
 * Results:
 *      A standard Tcl result; TCL_ERROR when running on RaspberryPi
 *      with and error message.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int Raspi_complain(Tcl_Interp *interp)
{
  FILE *fp;

  fp = fopen("/proc/cpuinfo", "r");
  if (fp != NULL) {
    char line[256];
    int check = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
      check = strncmp(line, "Hardware", 8) == 0;
      if (check) {
        break;
      }
    }
    fclose(fp);
    if (!check || strstr(line, "BCM2") == NULL) {
      return TCL_OK;
    }
  }
  Tcl_SetResult(interp, "unsupported hardware", TCL_STATIC);
  return TCL_ERROR;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tk_check --
 *
 *      Lazy load Tk and check for its availability.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      Tk package may be loaded.
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
 * DoEventCallback --
 *
 *      Perform callback given libVLCEvent.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Many since Tcl code is evaluated.
 *
 *----------------------------------------------------------------------
 */

static void DoEventCallback(libVLCData *p, libVLCEvent *e)
{
  if (p->nCmdObjs > 0 && p->cmdObjs != NULL) {
    Tcl_Interp *interp = p->interp;
    Tcl_InterpState state;
    int i, ret, nCmdObjs;
    Tcl_Obj **cmdObjs, *list;
    const char *evname;

    if (p->repeat && (e->type == EV_STATE_CHANGED)) {
      if (libvlc_media_player_get_state(p->media_player) == libvlc_Ended) {
        if (p->file_name != NULL) {
          char *filename = Tcl_GetString(p->file_name);
          libvlc_media_t *media;

          if(p->is_location) {
            media = libvlc_media_new_location(p->vlc_inst, filename);
          } else {
            media = libvlc_media_new_path(p->vlc_inst, filename);
          }
          if (media != NULL) {
            libvlc_media_player_set_media(p->media_player, media);
    #if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
            int status =
                    libvlc_media_parse_with_options(media,
                                libvlc_media_parse_local, -1);

                if (status < 0) {
                    libvlc_media_release(media);
                }
    #else
            libvlc_media_parse(media);
    #endif
            libvlc_media_player_play(p->media_player);
            libvlc_media_release(media);
          }
        }
      }
    }
    Tcl_Preserve(p);
    Tcl_Preserve(interp);
    state = Tcl_SaveInterpState(interp, TCL_OK);
    nCmdObjs = p->nCmdObjs;
    cmdObjs = p->cmdObjs;
    p->nCmdObjs = 0;
    p->cmdObjs = NULL;
    list = Tcl_NewListObj(nCmdObjs, cmdObjs);
    switch (e->type) {
      case EV_MEDIA_CHANGED:
        evname = "media";
        break;
      case EV_STATE_CHANGED:
        evname = "state";
        break;
      case EV_TIME_CHANGED:
        evname = "time";
        break;
      case EV_POS_CHANGED:
        evname = "position";
        break;
      case EV_AUDIO_CHANGED:
        evname = "audio";
        break;
      case EV_NEW_FRAME:
        evname = "frame";
        break;
      default:
        evname = "unknown";
        break;
    }
    Tcl_ListObjAppendElement(NULL, list, Tcl_NewStringObj(evname, -1));
    Tcl_IncrRefCount(list);
    ret = Tcl_GlobalEvalObj(interp, list);
    Tcl_DecrRefCount(list);
    if (ret != TCL_OK) {
      Tcl_AddErrorInfo(interp, "\n    (tkvlc event callback)");
      Tcl_BackgroundException(interp, ret);
    }
    if (p->cmdObjs != NULL) {
      /* new callback installed */
      for (i = 0; i < nCmdObjs; i++) {
          Tcl_DecrRefCount(cmdObjs[i]);
      }
      ckfree(cmdObjs);
    } else {
      p->nCmdObjs = nCmdObjs;
      p->cmdObjs = cmdObjs;
    }
    Tcl_RestoreInterpState(interp, state);
    Tcl_Release(interp);
    Tcl_Release(p);
  }
}

/*
 *----------------------------------------------------------------------
 *
 * libVLChandlerTcl --
 *
 *      Procedure called in Tcl thread to report a media player event.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      A Tcl callback is evaluated.
 *
 *----------------------------------------------------------------------
 */

static int libVLChandlerTcl(Tcl_Event *ev, int flags)
{
  libVLCEvent *e = (libVLCEvent *) ev;

  if (e->p != NULL) {
    libVLCData *p = e->p;
    libVLCEvent *this, *prev;

    /* remove event from event */
    this = p->ev_queue;
    prev = NULL;
    while (this != NULL) {
      if (this == e) {
        if (prev != NULL) {
          prev->next = this->next;
        } else {
          p->ev_queue = this->next;
        }
        break;
      }
      prev = this;
      this = this->next;
    }
    /* invoke callback, if any */
    DoEventCallback(p, e);
  }
  return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * libVLChandler --
 *
 *      Procedure called in libvlc context to report a media player
 *      event.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      A Tcl event is queued and the thread owning the media player
 *      is alerted.
 *
 *----------------------------------------------------------------------
 */

static void libVLChandler(const struct libvlc_event_t *ev, void *clientData)
{
  libVLCData *p = (libVLCData *) clientData;
  libVLCEvent *e;
  int type;

  if (p->media_player == NULL) {
    return;
  }
  switch (ev->type) {
    case libvlc_MediaPlayerMediaChanged:
      type = EV_MEDIA_CHANGED;
      break;
    case libvlc_MediaPlayerNothingSpecial:
    case libvlc_MediaPlayerOpening:
    case libvlc_MediaPlayerBuffering:
    case libvlc_MediaPlayerPlaying:
    case libvlc_MediaPlayerPaused:
    case libvlc_MediaPlayerStopped:
    case libvlc_MediaPlayerForward:
    case libvlc_MediaPlayerBackward:
    case libvlc_MediaPlayerEndReached:
    case libvlc_MediaPlayerEncounteredError:
      type = EV_STATE_CHANGED;
      break;
    case libvlc_MediaPlayerTimeChanged:
      type = EV_TIME_CHANGED;
      break;
    case libvlc_MediaPlayerPositionChanged:
      type = EV_POS_CHANGED;
      break;
    case libvlc_MediaPlayerMuted:
    case libvlc_MediaPlayerUnmuted:
    case libvlc_MediaPlayerAudioVolume:
    case libvlc_MediaPlayerAudioDevice:
      type = EV_AUDIO_CHANGED;
      break;
    default:
      return;
  }
  e = (libVLCEvent *) ckalloc(sizeof(*e));
  e->header.proc = libVLChandlerTcl;
  e->header.nextPtr = NULL;
  e->type = type;
  e->f = NULL;
  e->p = p;
  Tcl_MutexLock(&p->ev_lock);
  e->next = p->ev_queue;
  p->ev_queue = e;
  Tcl_ThreadQueueEvent(p->tid, &e->header, TCL_QUEUE_TAIL);
  Tcl_ThreadAlert(p->tid);
  Tcl_MutexUnlock(&p->ev_lock);
}

/*
 *----------------------------------------------------------------------
 *
 * libVLCready --
 *
 *      Procedure called by the Tcl event mechanism. The event
 *      indicates a filled video buffer which is copied to the
 *      photo image.
 *
 * Results:
 *      Always true (event handled).
 *
 * Side effects:
 *      Playback is stopped when the photo image is invalid.
 *      A frame event callback is invoked.
 *
 *----------------------------------------------------------------------
 */

static int libVLCready(Tcl_Event *ev, int flags)
{
  libVLCEvent *e = (libVLCEvent *) ev;
  libVLCFrame *f = e->f;
  libVLCData *p;
  Tcl_Interp *interp;
  Tk_PhotoHandle photo;
  int docb = 0;

  if (f == NULL) {
    /* about to tear down media player */
    return 1;
  }
  p = f->p;
  interp = p->interp;
  photo = Tk_FindPhoto(interp, Tcl_GetString(p->photo_name));
  if (photo == NULL) {
    libvlc_media_player_stop(p->media_player);
  } else if (libvlc_media_player_is_playing(p->media_player) == 1) {
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
      if (Tk_PhotoPutBlock(interp, photo, &blk, 0, 0, blk.width, blk.height,
               TK_PHOTO_COMPOSITE_SET) == TCL_OK) {
          docb = 1;
      }
    }
  }
  Tcl_ResetResult(interp);
  f->busy = 0;
  f->e = NULL;
  if (docb) {
    /* invoke callback, if any */
    DoEventCallback(p, e);
  }
  return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * libVLClock --
 *
 *      Procedure called in libvlc context when a new video frame is
 *      to be decoded.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
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
  p->frames[index].busy = 1;
  planes[0] = p->frames[index].pixels;
  return &p->frames[index];
}

/*
 *----------------------------------------------------------------------
 *
 * libVLCdisplay --
 *
 *      Procedure called by libvlc when a new video frame is
 *      to be displayed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      A Tcl event is queued and the thread owning the media player
 *      is alerted.
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
    /* other frame buffer still in use, drop frame */
    return;
  }
  e = (libVLCEvent *) ckalloc(sizeof(*e));
  e->header.proc = libVLCready;
  e->header.nextPtr = NULL;
  e->type = EV_NEW_FRAME;
  e->f = f;
  e->p = NULL;
  /* not queued to ev_queue but remembered in frame buffer */
  e->next = NULL;
  f->e = e;
  Tcl_ThreadQueueEvent(p->tid, &e->header, TCL_QUEUE_TAIL);
  Tcl_ThreadAlert(p->tid);
}

/*
 *----------------------------------------------------------------------
 *
 * libVLCstatestr --
 *
 *      Return current state as string.
 *
 * Results:
 *      String pointer.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static const char *libVLCstatestr(libVLCData *p)
{
  const char *string;
  libvlc_state_t state;

  /* get the current state of the media player (playing, paused, ...) */
  state = libvlc_media_player_get_state(p->media_player);
  if (state == libvlc_NothingSpecial) {
    string = "idle";
  } else if (state == libvlc_Opening) {
    string = "opening";
  } else if (state == libvlc_Buffering) {
    string = "buffering";
  } else if (state == libvlc_Playing) {
    string = "playing";
  } else if (state == libvlc_Paused) {
    string = "paused";
  } else if (state == libvlc_Stopped) {
    string = "stopped";
  } else if (state == libvlc_Ended) {
    string = "ended";
  } else if (state == libvlc_Error) {
    string = "error";
  } else {
    string = "unknown";
  }
  return string;
}

#endif


/*
 *----------------------------------------------------------------------
 *
 * libVLCObjCmd --
 *
 *      Tcl command to deal with the libvlc media player.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      Many depending on command arguments.
 *
 *----------------------------------------------------------------------
 */

int libVLCObjCmd(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  libVLCData *pVLC = (libVLCData *) cd;
  int choice;
  int rc = TCL_OK;

  static const char *VLC_strs[] = {
    "open", "openurl", "play", "pause", "stop", "isplaying",
    "mute", "volume", "duration", "time", "position",
    "rate", "isseekable", "state", "version", "destroy",
#ifdef USE_TK_PHOTO
    "event", "repeat", "info",
#endif
    NULL
  };
  enum VLC_enum {
    TKVLC_OPEN, TKVLC_OPENURL, TKVLC_PLAY, TKVLC_PAUSE, TKVLC_STOP, TKVLC_ISPLAYING,
    TKVLC_MUTE, TKVLC_VOLUME, TKVLC_DURATION, TKVLC_TIME, TKVLC_POSITION,
    TKVLC_RATE, TKVLC_ISSEEKABLE, TKVLC_STATE, TKVLC_VERSION, TKVLC_DESTROY,
#ifdef USE_TK_PHOTO
    TKVLC_EVENT, TKVLC_REPEAT, TKVLC_INFO,
#endif
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
        Tcl_DString ds;
        int status = 0;
        libvlc_media_t *media;

        if( objc != 3 ){
            Tcl_WrongNumArgs(interp, 2, objv, "filename");
            return TCL_ERROR;
        }

        filename = Tcl_TranslateFileName(interp, Tcl_GetString(objv[2]), &ds);
        if (filename == NULL) {
            return TCL_ERROR;
        }

        media = libvlc_media_new_path(pVLC->vlc_inst, filename);
        if(media == NULL) {  // Is it necessary?
            Tcl_AppendResult(interp, "libvlc_media_new_path failed.", (char*)0);
            return TCL_ERROR;
        }

        libvlc_media_player_set_media(pVLC->media_player, media);
#if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
        status = libvlc_media_parse_with_options(media, libvlc_media_parse_local, -1);
        if (status < 0) {
            Tcl_DStringFree(&ds);
            Tcl_AppendResult(interp, "libvlc_media_parse_with_options failed.", (char*)0);
            return TCL_ERROR;
        }
#else
        libvlc_media_parse(media);
#endif

        if (pVLC->file_name != NULL) {
            Tcl_DecrRefCount(pVLC->file_name);
        }
        pVLC->file_name = Tcl_NewStringObj(filename, -1);
        Tcl_IncrRefCount(pVLC->file_name);
        Tcl_DStringFree(&ds);
        pVLC->is_location = 0;
        libvlc_media_player_play(pVLC->media_player); // Play media
        libvlc_media_release(media);

        break;
    }

    case TKVLC_OPENURL: {
        char *filename = NULL;
        int status = 0;
        libvlc_media_t *media;

        if (objc != 3){
            Tcl_WrongNumArgs(interp, 2, objv, "url");
            return TCL_ERROR;
        }

        filename = Tcl_GetString(objv[2]);
        if (filename == NULL) {
            return TCL_ERROR;
        }

        media = libvlc_media_new_location(pVLC->vlc_inst, filename);
        if(media == NULL) {  // Is it necessary?
            Tcl_AppendResult(interp, "libvlc_media_new_path failed.", (char*)0);
            return TCL_ERROR;
        }

        libvlc_media_player_set_media(pVLC->media_player, media);
#if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
        status = libvlc_media_parse_with_options(media, libvlc_media_parse_local, -1);
        if (status < 0) {
            Tcl_AppendResult(interp, "libvlc_media_parse_with_options failed.", (char*)0);
            return TCL_ERROR;
        }
#else
        libvlc_media_parse(media);
#endif

        if (pVLC->file_name != NULL) {
            Tcl_DecrRefCount(pVLC->file_name);
        }
        pVLC->file_name = Tcl_NewStringObj(filename, -1);
        Tcl_IncrRefCount(pVLC->file_name);
        pVLC->is_location = 1;
        libvlc_media_player_play(pVLC->media_player); // Play media
        libvlc_media_release(media);

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

    case TKVLC_MUTE: {
      int mute;

      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?value?");
        return TCL_ERROR;
      }
      if (objc > 2) {
        if (Tcl_GetBooleanFromObj(interp, objv[2], &mute) != TCL_OK) {
           return TCL_ERROR;
        }
        libvlc_audio_set_mute(pVLC->media_player, mute);
      } else {
        mute = libvlc_audio_get_mute(pVLC->media_player);
        if (mute < 0) {
          mute = 0;
        }
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(mute));
      }
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

        tm = libvlc_media_player_get_length(pVLC->media_player);
        if (tm != -1) {
            Tcl_SetResult(interp, "no media", TCL_STATIC);
            return TCL_ERROR;
        }
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

#ifdef USE_TK_PHOTO
    case TKVLC_EVENT: {
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?cmd?");
        return TCL_ERROR;
      }
      if (objc == 2) {
        Tcl_SetObjResult(interp,
        Tcl_NewListObj(pVLC->nSavedCmdObjs, pVLC->savedCmdObjs));
      } else {
        Tcl_Obj **cmdObjs;
        int i, nCmdObjs;

        if (Tcl_ListObjGetElements(interp, objv[2], &nCmdObjs, &cmdObjs)
            != TCL_OK) {
          return TCL_ERROR;
        }
        if (nCmdObjs <= 0) {
          Tcl_SetResult(interp, "empty command", TCL_STATIC);
          return TCL_ERROR;
        }
        if (pVLC->savedCmdObjs != NULL) {
          for (i = 0; i < pVLC->nSavedCmdObjs; i++) {
            Tcl_DecrRefCount(pVLC->savedCmdObjs[i]);
          }
          ckfree(pVLC->savedCmdObjs);
        }
        pVLC->nCmdObjs = nCmdObjs;
        pVLC->cmdObjs = (Tcl_Obj **) ckalloc(nCmdObjs * sizeof(Tcl_Obj *));
        for (i = 0; i < nCmdObjs; i++) {
          pVLC->cmdObjs[i] = cmdObjs[i];
          Tcl_IncrRefCount(pVLC->cmdObjs[i]);
        }
        pVLC->nSavedCmdObjs = pVLC->nCmdObjs;
        pVLC->savedCmdObjs = pVLC->cmdObjs;
      }
      break;
    }

    case TKVLC_REPEAT: {
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?flag?");
        return TCL_ERROR;
      }
      if (objc > 2) {
        int flag;

        if (Tcl_GetBooleanFromObj(interp, objv[2], &flag) != TCL_OK) {
          return TCL_ERROR;
        }
        pVLC->repeat = flag;
      } else {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(pVLC->repeat));
      }
      break;
    }

    case TKVLC_INFO: {
      Tcl_Obj *list = Tcl_NewListObj(0, NULL);

#define TLOAE(elem) Tcl_ListObjAppendElement(NULL, list, (elem))
#define TLOAE_STR(s) TLOAE(Tcl_NewStringObj((s), -1))
#define TLOAE_INT(i) TLOAE(Tcl_NewIntObj((i)))
#define TLOAE_DBL(d) TLOAE(Tcl_NewDoubleObj((d)))
#define TLOAE_BOOL(b) TLOAE(Tcl_NewBooleanObj((b)))

      TLOAE_STR("media");
      if (pVLC->file_name != NULL) {
         TLOAE(pVLC->file_name);
      } else {
         TLOAE(Tcl_NewObj());
      }
      TLOAE_STR("mode");
      TLOAE_STR((pVLC->photo_name != NULL) ? "photo" : "window");
      TLOAE_STR("target");
      if (pVLC->photo_name != NULL) {
        TLOAE(pVLC->photo_name);
      } else {
        char buffer[64];

        sprintf(buffer, "0x%" TCL_LL_MODIFIER "x", pVLC->window_id);
        TLOAE_STR(buffer);
      }
      TLOAE_STR("width");
      TLOAE_INT(pVLC->width);
      TLOAE_STR("height");
      TLOAE_INT(pVLC->height);
      TLOAE_STR("state");
      TLOAE_STR(libVLCstatestr(pVLC));
      TLOAE_STR("mute");
      TLOAE_BOOL(libvlc_audio_get_mute(pVLC->media_player) > 0);
      TLOAE_STR("volume");
      TLOAE_INT(libvlc_audio_get_volume (pVLC->media_player));
      TLOAE_STR("duration");
      TLOAE_DBL((double)
      libvlc_media_player_get_length(pVLC->media_player) / 1000.0);
      TLOAE_STR("time");
      TLOAE_DBL((double)
      libvlc_media_player_get_time(pVLC->media_player) / 1000.0);
      TLOAE_STR("position");
      TLOAE_DBL(libvlc_media_player_get_position(pVLC->media_player));
      TLOAE_STR("rate");
      TLOAE_DBL(libvlc_media_player_get_rate(pVLC->media_player));
      TLOAE_STR("seekable");
      TLOAE_BOOL(libvlc_media_player_is_seekable(pVLC->media_player) > 0);
      TLOAE_STR("repeat");
      TLOAE_BOOL(pVLC->repeat);

#undef TLOAE
#undef TLOAE_STR
#undef TLOAE_INT
#undef TLOAE_DBL
#undef TLOAE_BOOL

      Tcl_SetObjResult(interp, list);
      break;
    }
#endif

  } /* End of the SWITCH statement */

  return rc;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeData --
 *
 *  Callback to cleanup libVLCData struct.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Resources are released.
 *
 *----------------------------------------------------------------------
 */

static void FreeData(char *clientData)
{
  libVLCData *p = (libVLCData *) clientData;
  libvlc_media_player_t *m;
#ifdef USE_TK_PHOTO
  libVLCEvent *e;
  int i;
#endif

#ifdef USE_TK_PHOTO
  /* invalidate event callback */
  if (p->cmdObjs != NULL) {
    for (i = 0; i < p->nCmdObjs; i++) {
      Tcl_DecrRefCount(p->cmdObjs[i]);
    }
    ckfree(p->cmdObjs);
  }
  p->nCmdObjs = p->nSavedCmdObjs = 0;
  p->cmdObjs = p->savedCmdObjs = NULL;
#endif
  m = p->media_player;
  p->media_player = NULL;
  /* stop media player */
  if (m != NULL) {
    libvlc_media_player_stop(m);
  }
#ifdef USE_TK_PHOTO
  /* invalidate queued events */
  Tcl_MutexLock(&p->ev_lock);
  e = p->ev_queue;
  while (e != NULL) {
    e->p = NULL;
    e = e->next;
  }
  Tcl_MutexUnlock(&p->ev_lock);
  /* invalidate queued frames */
  if (p->frames[0].e != NULL) {
    p->frames[0].e->f = NULL;
  }
  if (p->frames[1].e != NULL) {
    p->frames[1].e->f = NULL;
  }
#endif
  /* release media player */
  if (m != NULL) {
    libvlc_media_player_stop(m);
    libvlc_media_player_release(m);
  }
  libvlc_release(p->vlc_inst);
#ifdef USE_TK_PHOTO
  if (p->photo_name != NULL) {
    Tcl_DecrRefCount(p->photo_name);
  }
#endif
  if (p->file_name != NULL) {
    Tcl_DecrRefCount(p->file_name);
  }
 
#ifdef USE_TK_PHOTO
  /* cleanup frame buffers */
  if (p->frames[0].pixels != NULL) {
    ckfree(p->frames[0].pixels);
  }
  if (p->frames[1].pixels != NULL) {
    ckfree(p->frames[1].pixels);
  }
  Tcl_MutexFinalize(&p->ev_lock);
#endif
  ckfree(p);
}

/*
 *----------------------------------------------------------------------
 *
 * libVLCObjCmdDeleted --
 *
 *  Destructor of libvlc media player object and command.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Resources are released.
 *
 *----------------------------------------------------------------------
 */

static void libVLCObjCmdDeleted(ClientData clientData)
{
  Tcl_EventuallyFree((char *) clientData, FreeData);
}


/*
 *----------------------------------------------------------------------
 *
 * TKVLC_INIT --
 *
 *  Initialize a libvlc media player given window identifier
 *  or photo image name.
 *
 * Results:
 *  A standard Tcl result.
 *
 * Side effects:
 *  Memory and resources are allocated, a Tcl command is
 *  created to refer to the media player.
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
#ifdef USE_TK_PHOTO
    libvlc_event_manager_t *em;
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
    p->file_name = NULL;
    p->is_location = 0;

#ifdef USE_TK_PHOTO
    p->repeat = 0;
    p->tk_checked = 0;
    p->window_id = 0;
    p->photo_name = NULL;
    p->width = p->height = 0;
    p->tid = Tcl_GetCurrentThread();
    p->ev_lock = NULL;
    p->ev_queue = NULL;
    p->nCmdObjs = p->nSavedCmdObjs = 0;
    p->cmdObjs = p->savedCmdObjs = NULL;
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

#ifdef linux
      /*
       * Raspberry Pi: the mmal_vout (or whatever other component)
       * is unable to provide the proper callback behavior for
       * rendering to photo images. Thus we don't support it for now.
       */
      if (Raspi_complain(interp) != TCL_OK) {
        libvlc_media_player_release(p->media_player);
        libvlc_release(p->vlc_inst);
        ckfree(p);
        return TCL_ERROR;
      }
#endif

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

      em = libvlc_media_player_event_manager(p->media_player);
      libvlc_event_attach(em, libvlc_MediaPlayerMediaChanged, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerNothingSpecial, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerOpening, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerBuffering, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerPlaying, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerPaused, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerStopped, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerForward, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerBackward, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerEndReached, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, libVLChandler, p);

#if LIBVLC_VERSION_INT >= LIBVLC_VERSION(3, 0, 0, 0)
      libvlc_event_attach(em, libvlc_MediaPlayerMuted, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerUnmuted, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerAudioVolume, libVLChandler, p);
      libvlc_event_attach(em, libvlc_MediaPlayerAudioDevice, libVLChandler, p);
#endif
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
 *      Initialize the new package.
 *
 * Results:
 *      A standard Tcl result
 *
 * Side effects:
 *      The tkvlc package is created.
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
