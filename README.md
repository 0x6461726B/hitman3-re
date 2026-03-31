# hitman3-re

Runtime research tool for the Glacier 2 engine (Hitman 3), built to understand 
engine internals through DX12 hooking and memory analysis.

## What it does

- Hooks DX12 `Present` and `Signal` to inject an ImGui overlay at runtime
- Implements a component lookup system matching Glacier 2's internal registry
- Reverses and reimplements the engine's chained XOR encryption scheme used 
  for runtime value obfuscation
- Exposes actor positions, inventory state, HP, and camera data via the overlay

## Technical highlights

- **Component system**: Glacier 2 uses a key-based component lookup table with 
  two modes (inline SBO for small counts, heap-allocated for large). Implemented 
  in `utils.h` as `getComponent()`.
- **Encryption**: Engine encrypts sensitive fields (HP, ammo) using a 16-byte 
  block where bytes 0–7 are the value and bytes 8–15 are an encrypted pointer 
  to the hash/key. Reversed and implemented in `EncryptedField<T>`.
- **DX12 hook**: Resolves `Present` and `Signal` via dummy swap chain vtable. 
  Command queue identified via swap chain internal offset to avoid a race 
  condition where multiple queues signal before the correct one is known.

## Notes

Singleplayer only. Built for research purposes — understanding engine 
architecture, anti-tamper mechanisms, and DX12 hooking techniques.
