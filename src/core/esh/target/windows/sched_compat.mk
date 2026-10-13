# Temporary legacy scheduler backend selection

ifeq ($(SCHED_BACKEND),cortn)
SCHED_COMPAT_CSOURCES = cortn.c cortn_context.c sched_cortn_compat.c
SCHED_COMPAT_CPPFLAGS =
else ifeq ($(SCHED_BACKEND),thread)
SCHED_COMPAT_CSOURCES = sched_thread_compat.c
SCHED_COMPAT_CPPFLAGS = -DESH_SCHED_THREAD
endif
