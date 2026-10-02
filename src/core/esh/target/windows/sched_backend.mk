# Windows scheduler backend selection
ESH_SCHED_BACKEND ?= cortn

ifeq ($(ESH_SCHED_BACKEND),cortn)
ESH_SCHED_CSOURCES = cortn.c cortn_context.c sched_cortn.c
ESH_SCHED_ASOURCES = cortn_switch.S
else ifeq ($(ESH_SCHED_BACKEND),thread)
ESH_SCHED_CSOURCES = sched_thread.c
ESH_SCHED_ASOURCES =
ESH_SCHED_CPPFLAGS = -DESH_SCHED_THREAD
else
$(error Unknown ESH_SCHED_BACKEND: $(ESH_SCHED_BACKEND))
endif
