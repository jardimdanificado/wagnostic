# Mouse Platformer for Wagnostic

A simple side-scrolling platformer game where you control a character with the mouse.

## Features

- **Mouse Control**: Click anywhere to move the character to that position
- **Physics**: Gravity, friction, and collision detection
- **Side-scrolling**: Camera follows the player through a 1000-pixel wide world
- **Collectibles**: Golden coins that bob up and down
- **Particle Effects**: Sparkle effects when collecting coins
- **Parallax Background**: Mountains that move at different speeds

## How to Play

1. **Move**: Click the left mouse button to move the character toward the cursor
2. **Jump**: Click above the character to jump
3. **Collect**: Walk into golden coins to collect them
4. **Goal**: Collect all 6 coins to complete the level

## Controls

- **Left Mouse Button**: Move character / Jump
- **Mouse Position**: Target location for movement

## Building

```bash
make -C roms/mouse_platformer
```

## Running

```bash
./wagnostic mouse_platformer.wasm
```

## Technical Details

- **Resolution**: 320x240 pixels
- **Color Depth**: 16-bit (RGB565)
- **World Size**: 1000x240 pixels
- **Physics**: Custom implementation with gravity and friction
- **Collision**: AABB (Axis-Aligned Bounding Box) detection

## Game Objects

- **Player**: Blue character with red hat and white eyes
- **Platforms**: Green floating platforms and brown ground
- **Coins**: Golden collectibles with bobbing animation
- **Particles**: Yellow sparkle effects

## Code Structure

- `main.c`: Main game logic and rendering
- `Makefile`: Build configuration
- Uses Wagnostic SDK for WASM compilation
- Uses OliveC for 2D rendering primitives