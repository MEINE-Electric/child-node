# Future TODOs

- [ ] Make load state restoration atomic: keep one lock for the entire
  `restoreState()` operation so commands cannot interleave with the watchdog.
  Use private unlocked helpers; locking the current public methods again would
  deadlock.
