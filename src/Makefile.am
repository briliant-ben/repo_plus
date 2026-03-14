bin_PROGRAMS = rpp

rpp_SOURCES = \
    main.c \
    manifest.c \
    git_ops.c \
    color.c \
    tasks.c \
    cmd/cmd_misc.c \
    cmd/cmd_init.c \
    cmd/cmd_sync.c \
    cmd/cmd_status.c \
    cmd/cmd_forall.c \
    cmd/cmd_others.c \
    cmd/cmd_branches.c \
    cmd/cmd_checkout.c \
    cmd/cmd_info.c \
    cmd/cmd_rebase.c \
    cmd/cmd_prune.c \
    cmd/cmd_stage.c \
    cmd/cmd_cherry_pick.c \
    cmd/cmd_upload.c \
    cmd/cmd_download.c \
    cmd/cmd_grep.c

rpp_CFLAGS  = -Wall -Wextra -std=c11 $(AM_CFLAGS)
rpp_LDADD   = $(EXPAT_LIBS) $(PTHREAD_LIBS)

AM_CPPFLAGS = -I$(top_builddir)
