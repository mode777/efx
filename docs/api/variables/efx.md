[**EFX API**](../README.md)

***

[EFX API](../README.md) / efx

# Variable: efx

> `const` **efx**: [`Efx`](../interfaces/Efx.md)

The engine-provided script surface; the only global scripts use. It exposes
the runtime and lifecycle facilities (`log`, `quit`, the read-only `args`,
and the frame-hook registrations) directly, and groups the rest of the
engine into domain sub-namespaces: `graphics`, `math`, `io`, `physics`,
`keyboard`, `mouse`, `gamepad`, `window`, `audio`, `noise`, and `color`.

## Example

```js
// the smallest complete 3D scene (the "Hello Cube" sample)
efx.graphics.setClearColor([0.03, 0.04, 0.09, 1]);
efx.graphics.setCamera3D([0, 1.6, 4.2], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
cube.setSurfaceMaterial(0, {
  ambient:  { color: [0.12, 0.12, 0.16, 1] },
  diffuse:  { color: [1, 1, 1, 1] },
  specular: { color: [1, 1, 1, 1], shininess: 32 },
  emissive: { color: [0, 0, 0, 1] },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
  const model = efx.math.mat4.rotate(efx.math.mat4.identity(), t * 40, [0, 1, 0]);
  efx.graphics.drawMesh(cube, { transform: model, color: [0.95, 0.5, 0.2, 1] });
}
```
