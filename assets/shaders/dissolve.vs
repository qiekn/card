#version 330
// dissolve.vs — port of ref-balatro/resources/shaders/dissolve.fs (#ifdef VERTEX block).
// LÖVE GLSL 110 → raylib GLSL 330. See docs/src/architecture/shader-uniforms.md.
//
// Hover lift: pushing vertex.w (homogeneous W) makes the perspective divide
// "zoom in" the card slightly toward the cursor. NOT a z translation —
// keep this hack as-is.

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform vec2 screen_size;        // replaces love_ScreenSize.xy
uniform vec2 mouse_screen_pos;
uniform float hovering;
uniform float screen_scale;

out vec2 fragTexCoord;
out vec4 fragColor;

void main() {
  fragTexCoord = vertexTexCoord;
  fragColor = vertexColor;

  vec4 vp = vec4(vertexPosition, 1.0);

  if (hovering <= 0.0) {
    gl_Position = mvp * vp;
    return;
  }

  float mid_dist = length(vp.xy - 0.5 * screen_size)
                 / length(screen_size);
  vec2 mouse_offset = (vp.xy - mouse_screen_pos) / screen_scale;
  float scale = 0.2 * (-0.03 - 0.3 * max(0.0, 0.3 - mid_dist))
              * hovering * pow(length(mouse_offset), 2.0)
              / (2.0 - mid_dist);

  gl_Position = mvp * vp + vec4(0.0, 0.0, 0.0, scale);
}
