XBE_TITLE = RedFaction-Reconstruction
NXDK_DIR ?= /c/nxdk
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OUTPUT_DIR = $(PROJECT_ROOT)/build/xbox/disc
GEN_XISO = redfaction-diagnostic.iso
SRCS = $(PROJECT_ROOT)/src/core/vpp.c $(PROJECT_ROOT)/src/core/checksum.c $(PROJECT_ROOT)/src/core/level.c $(PROJECT_ROOT)/src/core/geometry.c $(PROJECT_ROOT)/src/core/preview.c $(PROJECT_ROOT)/src/core/image.c $(PROJECT_ROOT)/src/core/material.c $(PROJECT_ROOT)/src/platform/xbox/main.c $(PROJECT_ROOT)/src/platform/xbox/renderer.c
SHADER_OBJS = $(PROJECT_ROOT)/src/platform/xbox/preview_vertex.inl $(PROJECT_ROOT)/src/platform/xbox/preview_fragment.inl
SRCS += $(PROJECT_ROOT)/src/core/lightmap.c
SRCS += $(PROJECT_ROOT)/src/core/eye.c
SRCS += $(PROJECT_ROOT)/src/core/model.c
SRCS += $(PROJECT_ROOT)/src/core/motion.c
SRCS += $(PROJECT_ROOT)/src/core/timer.c
SRCS += $(PROJECT_ROOT)/src/core/movement.c
SRCS += $(PROJECT_ROOT)/src/core/collision.c
SRCS += $(PROJECT_ROOT)/src/core/turn.c
SRCS += $(PROJECT_ROOT)/src/core/entity.c
SRCS += $(PROJECT_ROOT)/src/core/weapon.c
SRCS += $(PROJECT_ROOT)/src/core/effect.c
SRCS += $(PROJECT_ROOT)/src/core/motion_file.c
SRCS += $(PROJECT_ROOT)/src/core/model_file.c
SRCS += $(PROJECT_ROOT)/src/core/entity_assets.c
SRCS += $(PROJECT_ROOT)/src/diagnostic/animation.c
SRCS += $(PROJECT_ROOT)/src/diagnostic/scene.c
CFLAGS += -std=c11 -O2 -Wall -Wextra -Werror -fstack-usage -I$(PROJECT_ROOT)/include
LDFLAGS += -map:main.map -entry:rf_diagnostic_start
include $(NXDK_DIR)/Makefile

# Windows clang emits D:/ dependency targets while this make uses /d/ paths.
# Explicit project-header prerequisites keep shared ABI edits from leaving stale
# objects until dependency paths are normalized throughout the toolchain.
$(filter %.obj,$(SRCS:.c=.obj)): $(wildcard $(PROJECT_ROOT)/include/rf/*.h) $(PROJECT_ROOT)/src/platform/xbox/renderer.h
$(filter %.obj,$(SRCS:.c=.obj)): $(PROJECT_ROOT)/platforms/xbox/Makefile
$(PROJECT_ROOT)/src/platform/xbox/renderer.obj: $(SHADER_OBJS)
