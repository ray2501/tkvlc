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

libvlc_media_player_t *media_player;
libvlc_instance_t *vlc_inst;
int initialize = 0;
TCL_DECLARE_MUTEX(myMutex);


static int TKVLC_INIT(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
    if( objc != 2 ){
      Tcl_WrongNumArgs(interp, 1, objv, "HWND");
      return TCL_ERROR;
    }

    Tcl_MutexLock(&myMutex);
    initialize = 1;
    Tcl_MutexUnlock(&myMutex);

    vlc_inst = libvlc_new(0, NULL);
    media_player = libvlc_media_player_new(vlc_inst);

    /*
     * Tcl side use "winfo id window" to give a low-level
     * platform-specific identifier for window.
     *
     * On Unix platforms, this is the X window identifier.
     * Under Windows, this is the Windows HWND.
     */
#ifdef _WIN32
    Tcl_GetIntFromObj(interp, objv[1], (int*)&hwnd);
    libvlc_media_player_set_hwnd(media_player, (void *) hwnd);
#else
    Tcl_GetIntFromObj(interp, objv[1], (int *) &drawable);
    libvlc_media_player_set_xwindow(media_player, (uint32_t) drawable);
#endif

  return TCL_OK;
}


static int TKVLC_OPEN(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
    char *filename = NULL;
    int len = 0;
    libvlc_media_t *media;
 
    if( objc != 2 ){
      Tcl_WrongNumArgs(interp, 1, objv, "filename");
      return TCL_ERROR;
    }

    if(initialize==0) {
        Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
        return TCL_ERROR;
    }

    filename = Tcl_GetStringFromObj(objv[1], &len);
    if( !filename || len < 1 ) {
          return TCL_ERROR;
    }

    media = libvlc_media_new_path(vlc_inst, filename);
    if(media == NULL) {  // Is it necessary?
       return TCL_ERROR;
    }

    libvlc_media_player_set_media(media_player, media);
    
    // Play media
    libvlc_media_player_play(media_player);

    libvlc_media_release(media);

    return TCL_OK;
}


static int TKVLC_PLAY(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  if( objc != 1 ){
    Tcl_WrongNumArgs(interp, 1, objv, 0);
    return TCL_ERROR;
  }

  if(initialize==0) {
      Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
      return TCL_ERROR;
  }

  if(libvlc_media_player_is_playing(media_player) == 0) {
    libvlc_media_player_play(media_player);
  }

  return TCL_OK;
}


static int TKVLC_PAUSE(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  if( objc != 1 ){
    Tcl_WrongNumArgs(interp, 1, objv, 0);
    return TCL_ERROR;
  }

  if(initialize==0) {
      Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
      return TCL_ERROR;
  }

  if(libvlc_media_player_is_playing(media_player) == 1) {
    libvlc_media_player_pause(media_player);
  }

  return TCL_OK;
}


static int TKVLC_STOP(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  if( objc != 1 ){
    Tcl_WrongNumArgs(interp, 1, objv, 0);
    return TCL_ERROR;
  }

  if(initialize==0) {
      Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
      return TCL_ERROR;
  }

  libvlc_media_player_stop(media_player);

  return TCL_OK;
}


static int TKVLC_ISPLAYING(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  Tcl_Obj *return_obj;

  if( objc != 1 ){
    Tcl_WrongNumArgs(interp, 1, objv, 0);
    return TCL_ERROR;
  }

  if(initialize==0) {
      Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
      return TCL_ERROR;
  }

  if(libvlc_media_player_is_playing(media_player) == 1) {
    return_obj = Tcl_NewBooleanObj(1);
  } else {
    return_obj = Tcl_NewBooleanObj(0);
  }

  Tcl_SetObjResult(interp, return_obj);

  return TCL_OK;
}

static int TKVLC_SETVOLUME(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  Tcl_Obj *return_obj;
  int volume = 0, result = 0;

  if( objc != 2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "volume");
    return TCL_ERROR;
  }

  if(initialize==0) {
      Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
      return TCL_ERROR;
  }

  if(Tcl_GetIntFromObj(interp, objv[1], &volume) != TCL_OK) {
    return TCL_ERROR;
  }

  // 0 if the volume was set, -1 if it was out of range
  result = libvlc_audio_set_volume(media_player, volume);
  if(result == 0) {
    return_obj = Tcl_NewBooleanObj(1);
  } else {
    return_obj = Tcl_NewBooleanObj(0);
  }

  Tcl_SetObjResult(interp, return_obj);

  return TCL_OK;
}

static int TKVLC_GETVOLUME(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  Tcl_Obj *return_obj;
  int result = 0;

  if( objc != 1 ){
    Tcl_WrongNumArgs(interp, 1, objv, 0);
    return TCL_ERROR;
  }

  if(initialize==0) {
      Tcl_AppendResult(interp, "Please execute tkvlc::init first!", (char*)0);
      return TCL_ERROR;
  }

  result = libvlc_audio_get_volume (media_player);
  return_obj = Tcl_NewIntObj(result);

  Tcl_SetObjResult(interp, return_obj);

  return TCL_OK;
}

static int TKVLC_DESTROY(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv)
{
  if( objc != 1 ){
    Tcl_WrongNumArgs(interp, 1, objv, 0);
    return TCL_ERROR;
  }

  Tcl_MutexLock(&myMutex);
  initialize = 0;
  Tcl_MutexUnlock(&myMutex);

  libvlc_media_player_release(media_player);
  libvlc_release(vlc_inst);

  return TCL_OK;
}


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

    Tcl_CreateObjCommand(interp, "tkvlc::open", (Tcl_ObjCmdProc *) TKVLC_OPEN,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::play", (Tcl_ObjCmdProc *) TKVLC_PLAY,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::pause", (Tcl_ObjCmdProc *) TKVLC_PAUSE,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::stop", (Tcl_ObjCmdProc *) TKVLC_STOP,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::isPlaying", (Tcl_ObjCmdProc *) TKVLC_ISPLAYING,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::setVolume", (Tcl_ObjCmdProc *) TKVLC_SETVOLUME,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::getVolume", (Tcl_ObjCmdProc *) TKVLC_GETVOLUME,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, "tkvlc::destroy", (Tcl_ObjCmdProc *) TKVLC_DESTROY,
       (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
