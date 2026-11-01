# Contributing

Issues and pull requests are welcome.

- Build and run the tests before sending a change: `cmake --build build && ctest --test-dir build`.
- Keep the hook relay (`hook/lumo-hook.c`) dependency-free and never let it block an agent.
- Nothing may approve, send or publish without an explicit click.
- Every looping animation must stop while it isn't visible (the island idles at 0 % CPU).
- Match the code around you: C++20, Qt 6, QML with the `Ui` singleton for colours and sizes.
