#ifndef INSTA360_PARAMS_HPP_B632C79B_4329_4935_8CD6_E130AB7F0CF5
#define INSTA360_PARAMS_HPP_B632C79B_4329_4935_8CD6_E130AB7F0CF5

#include <array>
#include <cmath>
#include "../caminfo_fwd.hpp"

namespace caminfo::insta360 {

struct Params {
    struct Lens {
        double fx, fy, cx, cy;
        double k1, k2, k3, p1, p2, xi;

        std::array<double, 2> project_point(double x, double y, double z) const noexcept;
    };

    Lens lens[2];
    int nb_lens = 0;

    bool joined;
    int width, height;    // Image size for single lens.
    bool selfie;          // If true, video0(or left side in joined video) is for lens1.
};


inline std::array<double, 2>
Params::Lens::project_point(double x, double y, double z) const noexcept
{
    const auto r = std::sqrt(x * x + y * y + z * z);
    const auto xn = x / (z + xi * r);
    const auto yn = y / (z + xi * r);

    const auto x2 = xn*xn;
    const auto y2 = yn*yn;
    const auto r2 = x2 + y2;
    const auto r4 = r2 * r2;
    const auto r6 = r4 * r2;

    return
    {
        cx + fx * (xn * (1.0 + k1*r2 + k2*r4 + k3*r6) + 2.0*p1*xn*yn + p2*(r2 + 2.0*x2)),
        cy + fy * (yn * (1.0 + k1*r2 + k2*r4 + k3*r6) + 2.0*p2*xn*yn + p1*(r2 + 2.0*y2))
    };
}

bool get_params(const CameraInfo& info, Params& out, bool with_crop = true) noexcept;

} // namespace caminfo::insta360

#endif /* INSTA360_PARAMS_HPP_B632C79B_4329_4935_8CD6_E130AB7F0CF5 */
