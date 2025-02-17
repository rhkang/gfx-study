## Shadow acne

Light-visible surface is incorrectly considered shadowed due to the subtraction-based biasing (Peter-panning).

### Normal Biasing

```hlsl
float adjustedBias = clamp(max(0.005, 0.01 * (1.0 - dot(N, L))), 0.0, 0.02);
```
Moves the sample point along the normal to avoid self-shadowing.
With slope-aware biasing, when the surface is facing the light, bias goes smaller(prevents shadow detachment). Clamp is just a restriction as shadows are not pushed too far off the surface.

### Depth Biasing

```hlsl
float bias = shadowBias;
if (shadowDepth < shadowCoord.z - bias)
{
    shadow = 0.0f;
}
```

After sampling, apply fixed depth bias to deal with small precision error as information is stored in discrete pixels.

### PCF
- Light leaking problem (White artifacts through sharp edge)
    - Adjust PCF weight distribution
        - weight based on distance (Gaussian or Poisson kernel)
    - Adjust PCF sample range dynamically
        - Near shadows get smaller PCF (sharp), Far shadows get larger PCF (soft)

## Resources

[Common Techniques to Improve Shadow Depth Maps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps)