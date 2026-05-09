#include <slate/extra/insta360_params.hpp>
#include <slate/camera_info.hpp>
#include "tuple_utils.hpp"
#include <cassert>

namespace slate::insta360 {

bool get_params(const CameraInfo& info, Params& out, bool with_crop) noexcept
{
    if (info.vendor != CameraVendor::Insta360)
        return false;

    const auto& map = info.extras;
    using namespace slate::types;

    try {
        out.width  = (int)map.get_ex<Int>(GroupId::VideoInfo, KeyId::Width);
        out.height = (int)map.get_ex<Int>(GroupId::VideoInfo, KeyId::Height);
        out.selfie = map.get_or<Bool>(false, GroupId::Metadata, "is_selfie");

        const auto& offset_v3 = map.get_ex<VecDouble>(GroupId::Metadata, "offset_v3");
        if (offset_v3.size() < 20)
            return false;
        out.nb_lens = offset_v3.size() >= 39? 2 : 1;

        const auto src_width  = map.get_ex<UInt>(GroupId::Metadata, "crop_info", "src_width");
        const auto src_height = map.get_ex<UInt>(GroupId::Metadata, "crop_info", "src_height");
        const auto dst_width  = with_crop? map.get_ex<UInt>(GroupId::Metadata, "crop_info", "dst_width") : src_width;
        const auto dst_height = with_crop? map.get_ex<UInt>(GroupId::Metadata, "crop_info", "dst_height") : src_height;

        // check if the video file is joined side-by-side
        if (double(out.width) / out.height / dst_width * dst_height > 1.9) {
            out.width /= 2;
            out.joined = true;
        } else {
            out.joined = false;
        }

        for (int i = 0; i < out.nb_lens; ++i) {
            auto& lens = out.lens[i];
            const auto [xi, fx, fy, cx, cy, yaw, pitch, roll, tx, ty, tz, k1, k2, k3, p1, p2, width_, height_] = util::range_as_tuple<18>(offset_v3, 1 + 19 * i);
            std::tie(lens.k1, lens.k2, lens.k3, lens.p1, lens.p2, lens.xi) = std::tie(k1, k2, k3, p1, p2, xi);

            // assert(("dafaq?", UInt(width_) == src_width * 2 && UInt(height_) == src_height));

            // Adjust f/c for cropping and zooming
            lens.cx                  = (i == 1 ? cx - width_ / 2 : cx) - (src_width - dst_width) / 2.0;
            lens.cy                  = cy - (src_height - dst_height) / 2.0;
            const auto zoom_ratio_w  = double(out.width) / dst_width;
            const auto zoom_ratio_h  = double(out.height) / dst_height;
            lens.cx                 *= zoom_ratio_w;
            lens.cy                 *= zoom_ratio_h;
            lens.fx                  = fx * zoom_ratio_w;
            lens.fy                  = fy * zoom_ratio_h;
        }
    }
    catch (const std::out_of_range&) {
        return false;
    }

    return true;
}

} // namespace slate::insta360
