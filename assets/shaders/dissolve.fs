#version 330
// dissolve.fs — port of ref-balatro/resources/shaders/dissolve.fs (effect / dissolve_mask).
// LÖVE GLSL 110 → raylib GLSL 330. See docs/src/architecture/shader-uniforms.md.
//
// LÖVE → raylib mapping:
//   extern <T>      → uniform <T>
//   number          → float
//   Image           → sampler2D
//   Texel(t, uv)    → texture(t, uv)
//   vec4 effect(..) → main() with `out vec4 finalColor`
//   gl_FragColor    → finalColor
//
// dissolve_mask is shared by 10 card-effect shaders (foil, holo, polychrome, ...).
// When more shaders are ported, extract this fn + the shared uniforms into
// assets/shaders/common.glsl and have a C++ shader loader splice it in.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;      // raylib default sampler
uniform vec4 colDiffuse;         // raylib default tint

uniform float dissolve;
uniform float time;
uniform vec4  texture_details;   // (sprite.x, sprite.y, atlas.px, atlas.py)
uniform vec2  image_details;     // atlas (w, h) in pixels
uniform bool  shadow;
uniform vec4  burn_colour_1;
uniform vec4  burn_colour_2;

out vec4 finalColor;

vec4 dissolve_mask(vec4 tex, vec2 texture_coords, vec2 uv) {
  if (dissolve < 0.001) {
    return vec4(shadow ? vec3(0.0) : tex.xyz,
                shadow ? tex.a * 0.3 : tex.a);
  }

  // Smoothstep-ish remap to slightly overshoot 0..1 so the mask never
  // pauses at endpoints.
  float adjusted_dissolve =
      (dissolve * dissolve * (3.0 - 2.0 * dissolve)) * 1.02 - 0.01;

  float t = time * 10.0 + 2003.0;
  vec2 floored_uv = floor(uv * texture_details.ba)
                  / max(texture_details.b, texture_details.a);
  vec2 uv_scaled_centered =
      (floored_uv - 0.5) * 2.3 * max(texture_details.b, texture_details.a);

  vec2 field_part1 = uv_scaled_centered
      + 50.0 * vec2(sin(-t / 143.6340), cos(-t / 99.4324));
  vec2 field_part2 = uv_scaled_centered
      + 50.0 * vec2(cos( t / 53.1532),  cos( t / 61.4532));
  vec2 field_part3 = uv_scaled_centered
      + 50.0 * vec2(sin(-t / 87.53218), sin(-t / 49.0000));

  float field = (1.0 + (
      cos(length(field_part1) / 19.483) +
      sin(length(field_part2) / 33.155) * cos(field_part2.y / 15.73) +
      cos(length(field_part3) / 27.193) * sin(field_part3.x / 21.92)
    )) / 2.0;

  vec2 borders = vec2(0.2, 0.8);

  float res = (0.5 + 0.5 * cos(adjusted_dissolve / 82.612
                              + (field - 0.5) * 3.14))
    - (floored_uv.x > borders.y
        ? (floored_uv.x - borders.y) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve
    - (floored_uv.y > borders.y
        ? (floored_uv.y - borders.y) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve
    - (floored_uv.x < borders.x
        ? (borders.x - floored_uv.x) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve
    - (floored_uv.y < borders.x
        ? (borders.x - floored_uv.y) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve;

  if (tex.a > 0.01 && burn_colour_1.a > 0.01 && !shadow
      && res < adjusted_dissolve + 0.8 * (0.5 - abs(adjusted_dissolve - 0.5))
      && res > adjusted_dissolve) {
    if (!shadow
        && res < adjusted_dissolve + 0.5 * (0.5 - abs(adjusted_dissolve - 0.5))
        && res > adjusted_dissolve) {
      tex.rgba = burn_colour_1.rgba;
    } else if (burn_colour_2.a > 0.01) {
      tex.rgba = burn_colour_2.rgba;
    }
  }

  return vec4(shadow ? vec3(0.0) : tex.xyz,
              res > adjusted_dissolve ? (shadow ? tex.a * 0.3 : tex.a) : 0.0);
}

void main() {
  vec4 tex = texture(texture0, fragTexCoord);
  vec2 uv = (fragTexCoord * image_details
             - texture_details.xy * texture_details.ba)
          / texture_details.ba;

  if (!shadow && dissolve > 0.01) {
    if (burn_colour_2.a > 0.01) {
      tex.rgb = tex.rgb * (1.0 - 0.6 * dissolve)
              + 0.6 * burn_colour_2.rgb * dissolve;
    } else if (burn_colour_1.a > 0.01) {
      tex.rgb = tex.rgb * (1.0 - 0.6 * dissolve)
              + 0.6 * burn_colour_1.rgb * dissolve;
    }
  }

  finalColor = dissolve_mask(tex, fragTexCoord, uv) * fragColor * colDiffuse;
}
