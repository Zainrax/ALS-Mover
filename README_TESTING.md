# ALS Mover Testing Guide

## Current Implementation Status

We now have a basic working implementation of ALS with the Mover plugin that can be tested in the editor.

### What's Implemented

1. **AAlsMoverCharacter** - Base character class with:
   - MoverComponent integration
   - Basic input handling (movement, look, jump)
   - State tracking (Stance, Gait, Rotation Mode)
   - Debug info function for testing

2. **UAlsGroundMovementMode** - Basic ground movement with:
   - Velocity-based movement
   - Simple acceleration/friction
   - Gravity support
   - Ground collision detection

3. **AAlsMoverTestGameMode** - Simple game mode for testing

### How to Test in Editor

1. **Create Input Actions** (Content Browser):
   - `IA_ALS_Move` (Vector2D)
   - `IA_ALS_Look` (Vector2D)
   - `IA_ALS_Jump` (Digital/Bool)
   - `IA_ALS_Run` (Digital/Bool)
   - `IA_ALS_Walk` (Digital/Bool)
   - `IA_ALS_Crouch` (Digital/Bool)

2. **Create Input Mapping Context**:
   - Name: `IMC_ALS_Default`
   - Add mappings:
     - Move → WASD (with appropriate modifiers)
     - Look → Mouse X/Y
     - Jump → Spacebar
     - Run → Left Shift
     - Walk → Left Alt
     - Crouch → Left Ctrl

3. **Create Blueprint Character**:
   - Right-click in Content Browser → Blueprint Class
   - Parent Class: `AAlsMoverCharacter`
   - Name: `BP_AlsMoverTestCharacter`
   - Open and configure:
     - Set Skeletal Mesh (use default mannequin or any character mesh)
     - In Details panel under "ALS Input", assign all the Input Actions
     - Set Input Mapping Context to `IMC_ALS_Default`

4. **Create or Use Test Game Mode**:
   - Create Blueprint from `AAlsMoverTestGameMode`
   - Or use it directly

5. **Test Level Setup**:
   - Create new level or use existing
   - World Settings → Game Mode Override → Your test game mode
   - Add Player Start
   - Add some geometry for testing (floor, ramps, obstacles)

6. **Debug Testing**:
   - In your character Blueprint, you can call `GetDebugInfo()` and print it
   - Or use console commands to display debug info during play

### Current Limitations

- Movement speed doesn't change with gait yet (always uses walk speed)
- Jump is tracked but not implemented in movement mode
- No animation integration
- No advanced ALS features (mantling, rolling, etc.)
- Basic physics only

### Next Steps for Testing

1. Verify basic movement works (WASD)
2. Check if character responds to mouse look
3. Test collision with world geometry
4. Monitor state changes using debug info
5. Check if gravity and ground detection work properly

### Console Commands for Testing

```
# Show debug info (if implemented)
showdebug mover

# Display collision
show collision

# Teleport commands for testing
teleport x y z
```

### Known Issues Resolved

- ✅ Fixed gameplay tag registration error
- ✅ Movement mode now properly created in BeginPlay
- ✅ Uses correct Mover API (QueueNextMode instead of SetMovementMode)

### Remaining Issues

- State access uses const_cast (temporary solution)
- No proper data type registration yet
- Movement modes are created at runtime instead of being Blueprint configurable

### Performance Notes

The current implementation is basic and focused on getting something testable. Performance optimization will come after core functionality is verified.