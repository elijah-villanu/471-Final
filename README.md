# 471 Final Project Submission
## To Compline and Run
With VS CODE on Windows:
CTRL + SHIFT + P for Command Pallete
Choose "CMAKE: Configure" and choose GCC as the compiler
CTRL + SHIFT + P for Command Pallete
Choose "CMAKE: Build" to build files into P3.exe (Used assignment 3's base code)
Press Play at bottom left to run exe

## Controls
Camera control
W - A - S - D: Forward, left, backward, right
Mouse Scroll (AS INSTRUCTED FROM PITCH AND YAW LAB 8). Trackpad would work best
G: Cinematic view, only works once

## Code Explanations
Please note, my collision implementation is not a conventional one. Collisions are used when I psuedorandomly generate the 6 sedans. I do a collision check where each sedan has a x/z-axis 2D circle, and I simply check if there's overlap. I ignore y as all cars are on the same y-level. If spawned cars are colliding, I simply move by a offset (try at most 5 times until not colliding) to where cars no longer clip.
