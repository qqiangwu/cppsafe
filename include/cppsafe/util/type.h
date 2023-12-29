#pragma once

#define DISALLOW_COPY(cls)                                                                                             \
    cls(const cls&) = delete;                                                                                          \
    cls& operator=(const cls&) = delete

#define DISALLOW_MOVE(cls)                                                                                             \
    cls(cls&&) = delete;                                                                                               \
    cls& operator=(cls&&) = delete

#define DISALLOW_COPY_AND_MOVE(cls)                                                                                    \
    DISALLOW_COPY(cls);                                                                                                \
    DISALLOW_MOVE(cls)
