#pragma once

// NOLINTBEGIN(cppcoreguidelines-macro-usage, bugprone-macro-parentheses)
#define DISALLOW_COPY(cls)                                                                                             \
    cls(const cls&) = delete;                                                                                          \
    cls& operator=(const cls&) = delete

#define DISALLOW_MOVE(cls)                                                                                             \
    cls(cls&&) = delete;                                                                                               \
    cls& operator=(cls&&) = delete

#define DISALLOW_COPY_AND_MOVE(cls)                                                                                    \
    DISALLOW_COPY(cls);                                                                                                \
    DISALLOW_MOVE(cls)

// NOLINTEND(cppcoreguidelines-macro-usage, bugprone-macro-parentheses)
