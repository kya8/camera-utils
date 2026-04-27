#ifndef VALUE_TYPES_HPP_D396FF12_6599_4802_B17A_B0D3746A6FCB
#define VALUE_TYPES_HPP_D396FF12_6599_4802_B17A_B0D3746A6FCB

#include <vector>
#include <array>
#include <tuple>
#include <string>
#include <cstdint>

// Commonly used data types for metadata.
namespace slate::types {

using String      = std::string;
using Int         = std::int64_t;
using UInt        = std::uint64_t;
using Bool        = bool;
using Double      = double;
using VecDouble   = std::vector<double>;
using VecFloat    = std::vector<float>;
using VecI32      = std::vector<std::int32_t>;
using VecI64      = std::vector<std::int64_t>;
using VecBytes    = std::vector<unsigned char>;

using Tuple2d     = std::tuple<double, double>;
using Tuple3d     = std::tuple<double, double, double>;
using Tuple4d     = std::tuple<double, double, double, double>;
using Tuple5d     = std::tuple<double, double, double, double, double>;
using Tuple2f     = std::tuple<float, float>;
using Tuple3f     = std::tuple<float, float, float>;
using Tuple4f     = std::tuple<float, float, float, float>;
using Tuple5f     = std::tuple<float, float, float, float, float>;

using Array2d     = std::array<double, 2>;
using Array3d     = std::array<double, 3>;
using Array4d     = std::array<double, 4>;
using Array5d     = std::array<double, 5>;
using Array2f     = std::array<float, 2>;
using Array3f     = std::array<float, 3>;
using Array4f     = std::array<float, 4>;
using Array5f     = std::array<float, 5>;

using GyroVec     = std::vector<Tuple4d>;
using AccVec      = std::vector<Tuple4d>;
using ExposureVec = std::vector<Tuple2d>;

using QuaternionVec      = std::vector<Tuple4d>;  // X, Y, Z, W
using TimedQuaternionVec = std::vector<Tuple5d>;

using GPSDataEntry = std::tuple<bool, double, double, double, double, double, double>; // is_acquired, timestamp, lat, lon, altitude, speed, track
using GPSDataVec   = std::vector<GPSDataEntry>;

} // namespace slate::types

#endif /* VALUE_TYPES_HPP_D396FF12_6599_4802_B17A_B0D3746A6FCB */
