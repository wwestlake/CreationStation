# Driver Package Notes

Yes, the driver should have its own installer package.

## Shipping plan

- build the driver in a dedicated pipeline step
- produce a driver package on its own
- let the main app installer either:
  - launch the driver installer first, or
  - chain it as a bundled prerequisite in a bootstrapper

## Recommended rule

Keep the driver install separate from the main app unless we have a strong reason to bundle everything into one bootstrapper.

That keeps driver failures from blocking normal app updates.
