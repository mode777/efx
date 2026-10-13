/* mat4.transform composition shared by both bindings (desktop quickjs wire
 * and the web bridge). One implementation keeps desktop and web results
 * byte-identical and matches the script-side mat4 helper conventions:
 * column-major float[16]-style layout in doubles, degrees, T·Ry·Rx·Rz·S. */
#include "math/efx_math.h"

#include <math.h>

void efx_math_trs(double out[16], double px, double py, double pz,
                  double yaw_deg, double pitch_deg, double roll_deg,
                  double sx, double sy, double sz) {
    const double rad = 3.141592653589793 / 180.0;
    /* R = Ry·Rx·Rz, column-major upper 3x3 (rotation applied to the vector
     * first); same convention as the script-side mat4.rotate */
    const double cy = cos(yaw_deg * rad), snY = sin(yaw_deg * rad);
    const double cp = cos(pitch_deg * rad), sp = sin(pitch_deg * rad);
    const double cr = cos(roll_deg * rad), sr = sin(roll_deg * rad);
    const double ry[9] = { cy, 0, -snY,
                           0, 1, 0,
                           snY, 0, cy };
    const double rx[9] = { 1, 0, 0,
                           0, cp, sp,
                           0, -sp, cp };
    const double rz[9] = { cr, sr, 0,
                           -sr, cr, 0,
                           0, 0, 1 };
    double r[9], a[9];
    for (int c = 0; c < 3; c++) {
        for (int row = 0; row < 3; row++) {
            r[c * 3 + row] = ry[row] * rx[c * 3] +
                             ry[3 + row] * rx[c * 3 + 1] +
                             ry[6 + row] * rx[c * 3 + 2];
        }
    }
    for (int c = 0; c < 3; c++) {
        for (int row = 0; row < 3; row++) {
            a[c * 3 + row] = r[row] * rz[c * 3] +
                             r[3 + row] * rz[c * 3 + 1] +
                             r[6 + row] * rz[c * 3 + 2];
        }
    }
    out[0] = a[0] * sx; out[1] = a[1] * sx; out[2] = a[2] * sx; out[3] = 0;
    out[4] = a[3] * sy; out[5] = a[4] * sy; out[6] = a[5] * sy; out[7] = 0;
    out[8] = a[6] * sz; out[9] = a[7] * sz; out[10] = a[8] * sz; out[11] = 0;
    out[12] = px; out[13] = py; out[14] = pz; out[15] = 1;
}
