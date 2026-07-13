# Bootstrapper Notes

If we decide to chain the driver installer from the main app installer, this is the place for that plan.

## Suggested order

1. Install the driver package
2. Verify the virtual endpoints appear
3. Launch the main app installer

## Why this stays separate

- driver install failures should not block app-only updates
- driver signing and elevation are easier to isolate
- we can update the app UI without forcing a driver rebuild every time
