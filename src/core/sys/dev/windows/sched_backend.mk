# Windows scheduler backend selection
SCHED_BACKEND ?= cortn

ifeq ($(SCHED_BACKEND),cortn)
SCHED_CSOURCES = cortn.c cortn_context.c sched_cortn.c
SCHED_ASOURCES = cortn_switch.S
else ifeq ($(SCHED_BACKEND),thread)
SCHED_CSOURCES = sched_thread.c
SCHED_ASOURCES =
SCHED_CPPFLAGS = -DSCHED_THREAD
else
$(error Unknown SCHED_BACKEND: $(SCHED_BACKEND))
endif
