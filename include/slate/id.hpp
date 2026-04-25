#ifndef ID_HPP_EF3DF2F3_2860_455D_BABE_4EFBCDBA265B
#define ID_HPP_EF3DF2F3_2860_455D_BABE_4EFBCDBA265B

#include "slate/export.h"
#include <string_view>

namespace slate {

// Well-known group id.
enum class GroupId {
    NormalizedMetadata, // Holds well-known metadata represented by KeyId.
    Metadata,
    SensorData,
    ProcessedData,
    VideoInfo,
    Other,
    Max_GroupId // sentinel
};

// Well-know key tag for commonly used types of metadata.
enum class KeyId {
    CameraModel,
    SubModel,
    SerialNumber,
    LensType,
    FirmwareVersion,
    StabilizationMode,
    HasStabilization,
    CameraRotation,
    LensParams,

    Width,
    Height,
    FPS,
    Duration,
    IsCFR,
    FrameCount,
    DisplayRotation,
    VideoTrackIds,

    GyroData,
    AccData,
    ExposureData,
    CameraQuaternionData,
    TimedCameraQuaternionData,
    TimelapseTimestamp,
    GpsData,
    Max_KeyId // sentinel
};

/**
 * Get a informative string of the given GroupId.
 */
SLATE_EXPORT [[nodiscard]] std::string_view to_string(GroupId id) noexcept;
/**
 * Get a informative string of the given KeyId.
 */
SLATE_EXPORT [[nodiscard]] std::string_view to_string(KeyId id) noexcept;

} // namespace slate

#endif /* ID_HPP_EF3DF2F3_2860_455D_BABE_4EFBCDBA265B */
