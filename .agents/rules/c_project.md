---
description: C Project Development Rules
---
# C Language Programming Rules

1.  **Language Standard**: Use GNU C99 or standard C99 unless explicitly required otherwise.
2.  **Style**:
    *   Use 4 spaces for indentation. Never use tabs.
    *   Functions should be small and do one thing well.
    *   Variables should use `snake_case`.
    *   Constants and macros should use `UPPER_SNAKE_CASE`.
    *   Use clear and descriptive variable names.
3.  **Memory Management**:
    *   All memory allocated with `malloc`/`calloc` must be explicitly freed.
    *   Check return values of `malloc`/`calloc`, as well as file operations and standard library calls.
    *   Follow the convention: whoever allocates memory must free it unless explicitly passing ownership.
4.  **Error Handling**:
    *   Functions should return an `int` error code (e.g., 0 for success, non-zero for error), and return data via pointer arguments if needed.
    *   Handle all potential error conditions cleanly.

