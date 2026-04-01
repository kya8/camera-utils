#include "varmap.hpp"
#include <sstream>
#include "helper_templates.hpp"

namespace caminfo {

std::string_view
to_string(GroupId id) noexcept
{
    switch(id) {
    case GroupId::NormalizedMetadata : return "NormalizedMetadata";
    case GroupId::Metadata           : return "Metadata";
    case GroupId::VideoInfo          : return "VideoInfo";
    case GroupId::SensorData         : return "SensorData";
    case GroupId::ProcessedData      : return "ProcessedData";
    case GroupId::Other              : return "Other";
    default: return "Unknown GroupId";
    }
}
std::string_view
to_string(KeyId id) noexcept
{
    switch(id) {
    case KeyId::CameraModel               : return "CameraModel";
    case KeyId::SubModel                  : return "SubModel";
    case KeyId::SerialNumber              : return "SerialNumber";
    case KeyId::LensType                  : return "LensType";
    case KeyId::FirmwareVersion           : return "FirmwareVersion";
    case KeyId::StabilizationMode         : return "StabilizationMode";
    case KeyId::HasStabilization          : return "HasStabilization";
    case KeyId::CameraRotation            : return "CameraRotation";
    case KeyId::LensParams                : return "LensParams";
    case KeyId::Width                     : return "Width";
    case KeyId::Height                    : return "Height";
    case KeyId::FPS                       : return "FPS";
    case KeyId::Duration                  : return "Duration";
    case KeyId::IsCFR                     : return "IsCFR";
    case KeyId::FrameCount                : return "FrameCount";
    case KeyId::DisplayRotation           : return "DisplayRotation";
    case KeyId::VideoTrackIds             : return "VideoTrackIds";
    case KeyId::GyroData                  : return "GyroData";
    case KeyId::AccData                   : return "AccData";
    case KeyId::ExposureData              : return "ExposureData";
    case KeyId::CameraQuaternionData      : return "CameraQuaternionData";
    case KeyId::TimedCameraQuaternionData : return "TimedCameraQuaternionData";
    case KeyId::TimelapseTimestamp        : return "TimelapseTimestamp";
    case KeyId::GpsData                   : return "GpsData";
    default: return "Unknown KeyId";
    }
}


namespace {

using Stream = std::ostream;

template<typename, typename = void>
constexpr int my_tuple_size = -1;

template<typename T>
constexpr int my_tuple_size<T, std::void_t<decltype(std::tuple_size<T>::value)>> = std::tuple_size<T>::value;

struct Formatter {
    std::size_t max_vec_elem = 50; // no limit if set to 0
    using Ret = std::string;

    template<class T, typename = std::enable_if_t< is_streamable_v<Stream, T>> >
    Stream& push(Stream& os, const T& arg) const
    {
        if constexpr (std::is_same_v<T, bool>)
            os << (arg? "true" : "false");
        else if constexpr (std::is_arithmetic_v<T>)
            os << +arg;
        else
            os << arg;
        return os;
    }

    template<class T>
    Stream& push(Stream& os, const std::vector<T>& arg) const
    {
        const auto max_vec_len = max_vec_elem / (my_tuple_size<T> < 0 ? 1 : my_tuple_size<T>);

        os << "Vec of " << arg.size() << ": [";
        const std::size_t max_print = (arg.size() > max_vec_len && max_vec_elem)? max_vec_len : arg.size();
        for (std::size_t i = 0; i < max_print; ++i) {
            if (i > 0) os << ", ";
            push(os, arg[i]);
        }
        if (arg.size() > max_print) {
            os << ", ...";
        }
        os << ']';
        return os;
    }

private:
    // helper for unrolling tuple-like
    template<typename Tuple, std::size_t ...Idx>
    void push_tuple_impl(Stream& os, const Tuple& tup, std::index_sequence<Idx...>) const {
        os << '(';
        ([&]{
            if constexpr (Idx > 0)
                os << ", ";
            os << std::get<Idx>(tup);
        }(), ...);
        os << ')';
    }

public:
    template<typename T, std::enable_if_t<(my_tuple_size<T> > 0), int> = 0>
    Stream& push(Stream& os, const T& tup) const {
        push_tuple_impl(os, tup, std::make_index_sequence<my_tuple_size<T>>{});
        return os;
    }

    Stream& push(Stream& os, const Array& array) const {
        os << "Array: [";
        for (std::size_t i = 0; i < array.size(); ++i) {
            if (i > 0) {
                os << ", ";
            }
            std::visit(
                [&] (const auto& val) {
                    push(os, val);
                }
                , array[i]);
        }
        os << ']';
        return os;
    }

    Stream& push(Stream& os, const VarMap& map) const {
        os << '{';
        bool first = true;
        for (const auto& [key, val] : map) {
            if (!first) {
                os << ", ";
            }
            first = false;
            os << to_string(key) << ": ";
            std::visit(
                [&] (const auto& v) {
                    push(os, v);
                }
                , val);
        }
        os << '}';
        return os;
    }

    Stream& push(Stream& os, const std::monostate&) const {
        os << "(Empty data)";
        return os;
    }

    template <typename T>
    Ret operator()(const T& v) const {
        if constexpr (std::is_convertible_v<T, Ret>) {
            return v;
        } else if constexpr (std::is_constructible_v<Ret, const T&>) {
            return Ret(v);
        } else {
            std::ostringstream os;
            push(os, v);
            return std::move(os).str(); // C++20 allows move out the string buffer
        }
    }

    Ret operator()(const std::monostate&) const
    {
        return "Empty data";
    }

#ifdef CAMINFO_ENABLE_ANY
    Ret operator()(const std::any&) const
    {
        return "Any";
    }
#endif
};

}

std::string
to_string(const Value& var, std::size_t max_vec_len) noexcept
{
#if 0
    const auto visitor = [](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;

        // unmatched branches are not instantiated inside templates.
        if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        }
        else if constexpr (is_streamable_v<ST, T>) {
            ST result;
            result << arg;
            return result.str();
        }
        else if constexpr (is_instantiation_of<T, std::vector>::value) {
        if constexpr (is_streamable_v<ST, typename T::value_type>) {
            const std::size_t max_print = arg.size() > 50 ? 50 : arg.size();
            ST result;
            result << "Vec of " << arg.size() << ": [";
            for (auto i=0u; i<max_print; ++i) {
                if (i>0) result << ", ";
                if constexpr (std::is_arithmetic_v<typename T::value_type>)
                    result << +arg[i]; // `+' for promotion
                // ...specialize for strings...
                else
                    result << arg[i];
            }
            if (arg.size() > max_print) {
                result << ", ...";
            }
            result << "]";
            return result.str();
        } else {
            return "Vec of unknown type";
        }
        }
        else {
            return "Unknown type";
        }
    };
#endif

    return std::visit(Formatter{max_vec_len}, var);
}

std::string_view
to_string(const Key& key) noexcept
{
    return std::visit(
        []<typename T>(const T& v) -> std::string_view {
            if constexpr (std::is_same_v<T, KeyId>) {
                return to_string(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else {
                static_assert(false);
            }
        }, key);
}

std::ostream& operator<<(std::ostream& os, const VarMap& map)
{
    return Formatter{0}.push(os, map);
}

std::ostream& operator<<(std::ostream& os, const Value& val)
{
    std::visit(
        [&](const auto& v) {
            Formatter{0}.push(os, v);
        }
    , val);

    return os;
}

} // namespace caminfo
