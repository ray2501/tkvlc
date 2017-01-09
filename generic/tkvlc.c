/*
 * tkvlc.c
 *
 * A demo to embed libVLC to a Tk toolkit frame widget
 * Copyright (C) Danilo Chang 2017
 */

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vlc/vlc.h>

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


#ifdef _WIN32
HWND hwnd;
#else
uint32_t drawable;
#endif

typedef struct libVLCData libVLCData;

struct libVLCData {
  libvlc_instance_t *vlc_inst;
  Tcl_Interp *interp;
  libvlc_media_player_t *media_player;
  libvlc_media_t *media;
};


int libVLCObjCmd(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  libVLCData *pVLC = (libVLCData *) cd;
  int choice;
  int rc = TCL_OK;

  static const char *VLC_strs[] = {
    "open",
    "play",
    "pause",
    "stop",
    "isPlaying",
    "setVolume",
    "getVolume",
    "duration",
    "getTime",
    "setTime",
    "getPosition",
    "setPosition",
    "isSeekable",
    "version",
    "destroy",
    0
  };

  enum VLC_enum {
    TKVLC_OPEN,
    TKVLC_PLAY,
    TKVLC_PAUSE,
    TKVLC_STOP,
    TKVLC_ISPLAYING,
    TKVLC_SETVOLUME,
    TKVLC_GETVOLUME,
    TKVLC_DURATION,
    TKVLC_GETTIME,
    TKVLC_SETTIME,
    TKVLC_GETPOSITION,
    TKVLC_SETPOSITION,
    TKVLC_ISSEEKABLE,
    TKVLC_VERSION,
    TKVLC_DESTROY,
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
            return TCL_ERROR;
        }

        libvlc_media_player_set_media(pVLC->media_player, pVLC->media);
        libvlc_media_parse(pVLC->media);  //get meta info

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

    case TKVLC_SETVOLUME: {
        Tcl_Obj *return_obj;
        int volume = 0, result = 0;

        if( objc != 3 ){
            Tcl_WrongNumArgs(interp, 2, objv, "volume");
            return TCL_ERROR;
        }

        if(Tcl_GetIntFromObj(interp, objv[2], &volume) != TCL_OK) {
            return TCL_ERROR;
        }

        // 0 if the volume was set, -1 if it was out of range
        result = libvlc_audio_set_volume(pVLC->media_player, volume);
        if(result == 0) {
            return_obj = Tcl_NewBooleanObj(1);
        } else {
            return_obj = Tcl_NewBooleanObj(0);
        }

        Tcl_SetObjResult(interp, return_obj);

        break;
    }

    case TKVLC_GETVOLUME: {
        Tcl_Obj *return_obj;
        int result = 0;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        result = libvlc_audio_get_volume (pVLC->media_player);
        return_obj = Tcl_NewIntObj(result);

        Tcl_SetObjResult(interp, return_obj);

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

    case TKVLC_GETTIME: {
        Tcl_Obj *return_obj;
        libvlc_time_t tm;
        double result;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        tm = libvlc_media_player_get_time(pVLC->media_player);
        result = (double) tm / 1000.0;
        return_obj = Tcl_NewDoubleObj(result);

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case TKVLC_SETTIME: {
        libvlc_time_t tm = 0;
        double settm;

        if( objc != 3 ){
            Tcl_WrongNumArgs(interp, 2, objv, "time");
            return TCL_ERROR;
        }

        if(Tcl_GetDoubleFromObj(interp, objv[2], &settm) != TCL_OK) {
            return TCL_ERROR;
        }

        // Set the movie time in ms and user should give the movie time in sec
        tm = (libvlc_time_t) settm * 1000;

        // Notice: not all formats and protocols support this
        libvlc_media_player_set_time(pVLC->media_player, tm);

        break;
    }

    case TKVLC_GETPOSITION: {
        Tcl_Obj *return_obj;
        float result;

        if( objc != 2 ){
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        result = libvlc_media_player_get_position(pVLC->media_player);
        return_obj = Tcl_NewDoubleObj((double) result);

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case TKVLC_SETPOSITION: {
        double pos;

        if( objc != 3 ){
            Tcl_WrongNumArgs(interp, 2, objv, "pos");
            return TCL_ERROR;
        }

        if(Tcl_GetDoubleFromObj(interp, objv[2], &pos) != TCL_OK) {
            return TCL_ERROR;
        }

        if(pos < 0.0 || pos > 1.0) {
            return TCL_ERROR;
        }

        // Set movie position as percentage between 0.0 and 1.0
        // This has no effect if playback is not enabled.
        // This might not work depending on the underlying input format
        // and protocol.
        libvlc_media_player_set_position(pVLC->media_player, (float) pos);

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

      libvlc_media_player_release(pVLC->media_player);
      libvlc_release(pVLC->vlc_inst);

      Tcl_Free((char *)pVLC);
      pVLC = NULL;

      Tcl_DeleteCommand(interp, Tcl_GetStringFromObj(objv[0], 0));
      break;
    }

  } /* End of the SWITCH statement */

  return rc;
}


static int TKVLC_INIT(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
    const char *zArg;
    libVLCData *p;

    if( objc != 3 ) {
      Tcl_WrongNumArgs(interp, 2, objv, "HANDLE HWND");
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

    p->vlc_inst = libvlc_new(0, NULL);
    p->media_player = libvlc_media_player_new(p->vlc_inst);

    /*
     * Tcl side use "winfo id window" to give a low-level
     * platform-specific identifier for window.
     *
     * On Unix platforms, this is the X window identifier.
     * Under Windows, this is the Windows HWND.
     */
#ifdef _WIN32
    Tcl_GetIntFromObj(interp, objv[2], (int*)&hwnd);
    libvlc_media_player_set_hwnd(p->media_player, (void *) hwnd);
#else
    Tcl_GetIntFromObj(interp, objv[2], (int *) &drawable);
    libvlc_media_player_set_xwindow(p->media_player, (uint32_t) drawable);
#endif

    zArg = Tcl_GetStringFromObj(objv[1], 0);
    Tcl_CreateObjCommand(interp, zArg, libVLCObjCmd, (char*)p, (Tcl_CmdDeleteProc *)NULL);

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
    if (Tcl_InitStubs(interp, "8.5", 0) == NULL) {
	return TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "tkvlc::init", (Tcl_ObjCmdProc *) TKVLC_INIT,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
