# Camera Controls
Translating/moving the camera is controlled by WASD:
W - Forward
S - Backward
A - Left
D - Right

Rotating the camera (pitch and yaw) is done with the mouse scroll callback (similar to lab 8). I was unsure if the assignment meant actual mouse movement or just scrolls.

# Cinematic Camera
To activate the cinematic camera, press the G key. I use the quad bezier curve with linear interpolation. Please note that once the cinematic is run, you cannot reset it (ie pressing G does nothing after).