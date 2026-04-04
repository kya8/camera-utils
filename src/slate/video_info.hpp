#ifndef VIDEO_INFO_HPP_C13B3A0C_9807_4C0C_8080_7D7CD3B8D7DD
#define VIDEO_INFO_HPP_C13B3A0C_9807_4C0C_8080_7D7CD3B8D7DD

namespace slate {

struct VideoInfo {
    double duration;         // duration of the video track, in seconds.
    double fps;
    long   nb_frames;
    int    width, height;
    int    display_rotation; // 0, 90, 180, 270 or -1 for others. Clockwise!
    bool   is_cfr;           // is constant framerate?
};

} /* namespace slate */

#endif /* VIDEO_INFO_HPP_C13B3A0C_9807_4C0C_8080_7D7CD3B8D7DD */
