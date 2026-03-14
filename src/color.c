#include "color.h"
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static int g_use_color = 0;

void color_init(int mode) {
  /* mode: 0=never, 1=always, 2=auto */
  if (mode == 1) {
    g_use_color = 1;
  } else if (mode == 0) {
    g_use_color = 0;
  } else {
    /* auto: check if stdout is a TTY */
    if (isatty(STDOUT_FILENO)) {
      g_use_color = 1;
    } else {
      g_use_color = 0;
    }
  }
}

const char *color_get(ColorSlot slot) {
  if (!g_use_color)
    return "";

  switch (slot) {
  case COLOR_HEADER:
    return ANSI_BOLD;
  case COLOR_ADDED:
    return ANSI_GREEN;
  case COLOR_CHANGED:
    return ANSI_RED;
  case COLOR_UNTRACKED:
    return ANSI_RED;
  case COLOR_NOBRANCH:
    return ANSI_RED;
  case COLOR_IMPORTANT:
    return ANSI_RED;
  default:
    return "";
  }
}

void color_fprintf(FILE *fp, ColorSlot slot, const char *fmt, ...) {
  const char *code = color_get(slot);
  if (code[0]) {
    fputs(code, fp);
  }

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  if (code[0]) {
    fputs(ANSI_RESET, fp);
  }
}

void color_printf(ColorSlot slot, const char *fmt, ...) {
  const char *code = color_get(slot);
  if (code[0]) {
    fputs(code, stdout);
  }

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  if (code[0]) {
    fputs(ANSI_RESET, stdout);
  }
}
