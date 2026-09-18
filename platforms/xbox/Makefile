XBE_TITLE = RedFaction-Reconstruction
NXDK_DIR ?= /c/nxdk
NXDK_SDL = y
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OUTPUT_DIR = $(PROJECT_ROOT)/build/xbox/disc
GEN_XISO = redfaction-diagnostic.iso
SRCS = $(PROJECT_ROOT)/src/core/audio.c $(PROJECT_ROOT)/src/core/vpp.c $(PROJECT_ROOT)/src/core/checksum.c $(PROJECT_ROOT)/src/core/level.c $(PROJECT_ROOT)/src/core/geometry.c $(PROJECT_ROOT)/src/core/preview.c $(PROJECT_ROOT)/src/core/image.c $(PROJECT_ROOT)/src/core/material.c $(PROJECT_ROOT)/src/platform/xbox/main.c $(PROJECT_ROOT)/src/platform/xbox/renderer.c
SHADER_OBJS = $(PROJECT_ROOT)/src/platform/xbox/particle_vertex.inl $(PROJECT_ROOT)/src/platform/xbox/particle_fragment.inl $(PROJECT_ROOT)/src/platform/xbox/preview_vertex.inl $(PROJECT_ROOT)/src/platform/xbox/preview_fragment.inl
SHADER_OBJS += $(PROJECT_ROOT)/src/platform/xbox/solid_fragment.inl $(PROJECT_ROOT)/src/platform/xbox/world_vertex.inl $(PROJECT_ROOT)/src/platform/xbox/skinned_vertex.inl $(PROJECT_ROOT)/src/platform/xbox/rigid_vertex.inl
SRCS += $(PROJECT_ROOT)/src/core/geomod.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_notify.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_solid_clip.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_piece_bank.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_subdivide.c
SRCS += $(PROJECT_ROOT)/src/core/debris_audio.c
SRCS += $(PROJECT_ROOT)/src/core/collision_composition.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_publication.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_authored_post.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_publication_binding.c
SRCS += $(PROJECT_ROOT)/src/core/checkpoint_file.c
SRCS += $(PROJECT_ROOT)/src/core/player_checkpoint.c
SRCS += $(PROJECT_ROOT)/src/core/checkpoint_placement.c
SRCS += $(PROJECT_ROOT)/src/core/authored_reset_placement.c
SRCS += $(PROJECT_ROOT)/src/core/composed_checkpoint.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_authored_identity.c
SRCS += $(PROJECT_ROOT)/src/core/authored_identity_capture.c
SRCS += $(PROJECT_ROOT)/src/core/authored_checkpoint_layout.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_publication_digest.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_collision_digest.c
SRCS += $(PROJECT_ROOT)/src/core/geomod_retained_material_digest.c
SRCS += $(PROJECT_ROOT)/src/core/authored_owner_extension.c $(PROJECT_ROOT)/src/core/clutter_gameplay.c $(PROJECT_ROOT)/src/core/clutter_damage.c
SRCS += $(PROJECT_ROOT)/src/platform/xbox/checkpoint_storage.c
SRCS += $(PROJECT_ROOT)/src/platform/xbox/checkpoint_fixture.c
SRCS += $(PROJECT_ROOT)/src/platform/xbox/input.c
SRCS += $(PROJECT_ROOT)/src/platform/xbox/audio.c
APU_BACKEND = $(PROJECT_ROOT)/build/nxaudio
ifeq ($(wildcard $(APU_BACKEND)/audio_core.c),)
$(error Run python tools/build_apu_probe.py --backend-only before the Xbox build)
endif
APU_SRCS = $(APU_BACKEND)/audio_core.c $(APU_BACKEND)/audio_buffer.c $(APU_BACKEND)/audio_voice.c
SRCS += $(APU_SRCS)
SRCS += $(PROJECT_ROOT)/src/core/event_cycle.c
SRCS += $(PROJECT_ROOT)/src/core/event.c
SRCS += $(PROJECT_ROOT)/src/core/campaign.c
SRCS += $(PROJECT_ROOT)/src/core/frame_clock.c
SRCS += $(PROJECT_ROOT)/src/core/object_registry.c
SRCS += $(PROJECT_ROOT)/src/core/lightmap.c
SRCS += $(PROJECT_ROOT)/src/core/eye.c
SRCS += $(PROJECT_ROOT)/src/core/model.c
SRCS += $(PROJECT_ROOT)/src/core/model_bounds.c
SRCS += $(PROJECT_ROOT)/src/core/motion.c
SRCS += $(PROJECT_ROOT)/src/core/timer.c
SRCS += $(PROJECT_ROOT)/src/core/random.c
SRCS += $(PROJECT_ROOT)/src/core/movement.c
SRCS += $(PROJECT_ROOT)/src/core/swim.c
SRCS += $(PROJECT_ROOT)/src/core/liquid_damage.c
SRCS += $(PROJECT_ROOT)/src/core/collision.c
SRCS += $(PROJECT_ROOT)/src/core/turn.c
SRCS += $(PROJECT_ROOT)/src/core/burn.c
SRCS += $(PROJECT_ROOT)/src/core/entity.c
SRCS += $(PROJECT_ROOT)/src/core/player.c
SRCS += $(PROJECT_ROOT)/src/core/physics.c
SRCS += $(PROJECT_ROOT)/src/core/grenade_flight.c $(PROJECT_ROOT)/src/core/grenade_throw.c
SRCS += $(PROJECT_ROOT)/src/core/hitscan_select.c $(PROJECT_ROOT)/src/core/weapon_precision.c
SRCS += $(PROJECT_ROOT)/src/core/weapon.c
SRCS += $(PROJECT_ROOT)/src/core/effect.c
SRCS += $(PROJECT_ROOT)/src/core/particle_pool.c
SRCS += $(PROJECT_ROOT)/src/core/level_particles.c
SRCS += $(PROJECT_ROOT)/src/core/visibility.c
SRCS += $(PROJECT_ROOT)/src/core/debris_visibility.c
SRCS += $(PROJECT_ROOT)/src/core/motion_file.c
SRCS += $(PROJECT_ROOT)/src/core/model_file.c
SRCS += $(PROJECT_ROOT)/src/core/entity_assets.c
SRCS += $(PROJECT_ROOT)/src/diagnostic/animation.c
SRCS += $(PROJECT_ROOT)/src/diagnostic/scene.c
CFLAGS += -DRF_IMAGE_XBOX_NATIVE -std=c11 -O2 -Wall -Wextra -Werror -fstack-usage -I$(PROJECT_ROOT)/include
RF_GEOMOD_EXPANDED_PROFILE ?= 0
ifeq ($(RF_GEOMOD_EXPANDED_PROFILE),1)
CFLAGS += -DRF_GEOMOD_CUT_LIMIT=16 -DRF_GEOMOD_WORK_VERTICES=9216 -DRF_GEOMOD_WORK_FACES=2048 -DRF_GEOMOD_PUBLICATION_VERTICES=9216 -DRF_GEOMOD_PUBLICATION_FACES=2048 -DRF_CHECKPOINT_FILE_MAX=262144 -DRF_GEOMOD_LIGHTMAP_LIMIT=2048 -DSCENE_TERRAIN_SOURCE_VERTICES=9216 -DSCENE_TERRAIN_FACES=2048 -DSCENE_TERRAIN_DRAW_BUDGET=1048576 -DSCENE_TERRAIN_CORE_BUDGET=2359296 -DSCENE_TERRAIN_ATLAS_BUDGET=1572864 -DSCENE_AUTHORED_DIGEST_BUDGET=1048576 -DSCENE_AUTHORED_WRITER_BUDGET=2097152 -DSCENE_DESTRUCTION_BUDGET=16777216
endif
# Connected destruction plus fragment extraction exceeds NXDK's default 64 KiB
# main-thread stack (native reset/save/reload/rocket replay). Reserve 128 KiB;
# cxbe copies the PE stack reserve into the XBE stack commit on stock 64 MiB.
LDFLAGS += -map:main.map -entry:rf_diagnostic_start -stack:131072
SRCS += $(PROJECT_ROOT)/src/core/player_weapon.c
include $(NXDK_DIR)/Makefile
$(APU_SRCS:.c=.obj): CFLAGS += -std=c23 -Wno-unused-function -I$(APU_BACKEND)/include
$(PROJECT_ROOT)/src/platform/xbox/audio.obj: CFLAGS += -std=c23 -I$(APU_BACKEND)/include

# Windows clang emits D:/ dependency targets while this make uses /d/ paths.
# Explicit project-header prerequisites keep shared ABI edits from leaving stale
# objects until dependency paths are normalized throughout the toolchain.
$(filter %.obj,$(SRCS:.c=.obj)): $(wildcard $(PROJECT_ROOT)/include/rf/*.h) $(PROJECT_ROOT)/src/platform/xbox/renderer.h
$(filter %.obj,$(SRCS:.c=.obj)): $(PROJECT_ROOT)/platforms/xbox/Makefile
$(PROJECT_ROOT)/src/platform/xbox/renderer.obj: $(SHADER_OBJS) $(PROJECT_ROOT)/src/platform/xbox/retained_models.h

$(PROJECT_ROOT)/src/platform/xbox/main.obj: $(wildcard $(PROJECT_ROOT)/tests/*_fixture.h)

# Scene implementation includes share the Windows dependency-path mismatch above.
# Recompile their owner when any include changes; otherwise a new executable can
# silently link stale gameplay code despite a successful incremental build.
$(PROJECT_ROOT)/src/diagnostic/scene.obj: $(wildcard $(PROJECT_ROOT)/src/diagnostic/*.inc)

# Profile switches invalidate all project objects, including shared ABI users.
$(filter %.obj,$(SRCS:.c=.obj)): $(PROJECT_ROOT)/build/xbox/geomod-profile.stamp
