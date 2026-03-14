#ifndef COLOR_H
#define COLOR_H

#include <stdio.h>

/* ANSI Escape Codes */
#define ANSI_RESET          "\033[0m"
#define ANSI_BOLD           "\033[1m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_WHITE          "\033[37m"

/* Repo specific color slots */
typedef enum {
  COLOR_NORMAL,
  COLOR_HEADER,      /* project line, branch */
  COLOR_ADDED,       /* A, C, R */
  COLOR_CHANGED,     /* M, T, D */
  COLOR_UNTRACKED,   /* ? */
  COLOR_NOBRANCH,    /* red */
  COLOR_IMPORTANT,   /* red */
  COLOR_MAX
} ColorSlot;

/* Initialize color subsystem */
void color_init(int color_ui_config);

/* Returns the ANSI code for a given slot, or empty string if colors are disabled */
const char *color_get(ColorSlot slot);

/* Helper to print with color */
void color_fprintf(FILE *fp, ColorSlot slot, const char *fmt, ...);
void color_printf(ColorSlot slot, const char *fmt, ...);

#endif /* COLOR_H */
