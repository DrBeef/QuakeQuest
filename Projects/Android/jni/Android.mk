
LOCAL_PATH:= $(call my-dir)

#--------------------------------------------------------
# libquakequest.so
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_CFLAGS			:= -std=c99
LOCAL_MODULE			:= quakequest
LOCAL_LDLIBS			:= -llog -landroid -lGLESv3 -lEGL		# include default libraries

LOCAL_C_INCLUDES := ../QuakeQuestSrc/ ../darkplaces/

LOCAL_SHARED_LIBRARIES	:= vrapi

SRC_SND_COMMON := \
	darkplaces/snd_main.c \
	darkplaces/snd_mem.c \
	darkplaces/snd_mix.c \
	darkplaces/snd_ogg.c \
	darkplaces/snd_wav.c \
	darkplaces/snd_modplug.c


###### Common objects and flags #####

# Common objects
SRC_COMMON := \
	darkplaces/cd_null.c \
	darkplaces/bih.c \
	darkplaces/cap_avi.c \
	darkplaces/cap_ogg.c \
	darkplaces/cd_shared.c \
	darkplaces/crypto.c \
	darkplaces/cl_collision.c \
	darkplaces/cl_demo.c \
	darkplaces/cl_dyntexture.c \
	darkplaces/cl_input.c \
	darkplaces/cl_main.c \
	darkplaces/cl_parse.c \
	darkplaces/cl_particles.c \
	darkplaces/cl_screen.c \
	darkplaces/cl_video.c \
	darkplaces/clvm_cmds.c \
	darkplaces/cmd.c \
	darkplaces/collision.c \
	darkplaces/common.c \
	darkplaces/console.c \
	darkplaces/csprogs.c \
	darkplaces/curves.c \
	darkplaces/cvar.c \
	darkplaces/dpsoftrast.c \
	darkplaces/dpvsimpledecode.c \
	darkplaces/filematch.c \
	darkplaces/fractalnoise.c \
	darkplaces/fs.c \
	darkplaces/ft2.c \
	darkplaces/utf8lib.c \
	darkplaces/gl_backend.c \
	darkplaces/gl_draw.c \
	darkplaces/gl_rmain.c \
	darkplaces/gl_rsurf.c \
	darkplaces/gl_textures.c \
	darkplaces/hmac.c \
	darkplaces/host.c \
	darkplaces/host_cmd.c \
	darkplaces/image.c \
	darkplaces/image_png.c \
	darkplaces/jpeg.c \
	darkplaces/keys.c \
	darkplaces/lhnet.c \
	darkplaces/libcurl.c \
	darkplaces/mathlib.c \
	darkplaces/matrixlib.c \
	darkplaces/mdfour.c \
	darkplaces/menu.c \
	darkplaces/meshqueue.c \
	darkplaces/mod_skeletal_animatevertices_sse.c \
	darkplaces/mod_skeletal_animatevertices_generic.c \
	darkplaces/model_alias.c \
	darkplaces/model_brush.c \
	darkplaces/model_shared.c \
	darkplaces/model_sprite.c \
	darkplaces/mvm_cmds.c \
	darkplaces/netconn.c \
	darkplaces/palette.c \
	darkplaces/polygon.c \
	darkplaces/portals.c \
	darkplaces/protocol.c \
	darkplaces/prvm_cmds.c \
	darkplaces/prvm_edict.c \
	darkplaces/prvm_exec.c \
	darkplaces/r_explosion.c \
	darkplaces/r_lerpanim.c \
	darkplaces/r_lightning.c \
	darkplaces/r_lasersight.c \
	darkplaces/r_modules.c \
	darkplaces/r_shadow.c \
	darkplaces/r_sky.c \
	darkplaces/r_sprites.c \
	darkplaces/sbar.c \
	darkplaces/snprintf.c \
	darkplaces/sv_demo.c \
	darkplaces/sv_main.c \
	darkplaces/sv_move.c \
	darkplaces/sv_phys.c \
	darkplaces/sv_user.c \
	darkplaces/svbsp.c \
	darkplaces/svvm_cmds.c \
	darkplaces/sys_shared.c \
	darkplaces/vid_shared.c \
	darkplaces/view.c \
	darkplaces/wad.c \
	darkplaces/world.c \
	darkplaces/zone.c

SRC_QUEST := \
	QuakeQuestSrc/argtable3.c \
	QuakeQuestSrc/QuakeQuest_SurfaceView.c \
	QuakeQuestSrc/VrCompositor.c \

LOCAL_SRC_FILES := \
	$(SRC_QUEST) \
	darkplaces/builddate.c \
	darkplaces/sys_linux.c \
	darkplaces/vid_android.c \
	darkplaces/thread_pthread.c \
	darkplaces/snd_android.c  \
	$(SRC_SND_COMMON) \
	$(SRC_COMMON)

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)