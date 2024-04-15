#include "camera_utils.hpp"
#include "mp4_merge.hpp"
#include "version.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>


int main_mp4_join(int argc, char** argv) noexcept
{
    std::vector<const char*> inputs;
    const char* output = nullptr;

    {
        bool print_version = false;
        bool err_flag = 0;
        for (int i = 1; i < argc; ++i) {
            if (!std::strcmp(argv[i], "-o")) {
                if (++i < argc) output = argv[i];
                else err_flag = 1;
            }
            else if (!std::strcmp(argv[i], "-V")) {
                print_version = true;
                break;
            }
            else {
                inputs.emplace_back(argv[i]);
            }
            if (err_flag) break;
        }

        if (print_version) {
            using namespace camera_utils::version;
            std::printf("Version %s, %s\n", COMMIT_HASH, COMMIT_DATE);
            return 0;
        }
        if (err_flag || !output || inputs.size() < 2) {
            std::puts("Usage: mp4_join <file_1> <file_2> [...] <-o output_file> [-V]");
            return 2;
        }
    }

    using namespace mp4utils;
    MergeResult ret;
    std::atomic<bool> done = false;
    std::atomic<int> prog = -1;
    int prog_prev = -1;

    const auto prog_cb = [&] (int prog_) {
        prog.store(prog_, std::memory_order_release);
    };
    std::thread worker {
        [&] {
            ret = merge_mp4(static_cast<int>(inputs.size()), inputs.data(), output, prog_cb);
            done.store(true, std::memory_order_release);
        }
    };

    static constexpr int busy_spin_cnt = 5;
    static constexpr int busy_spin_interval = 1;
    static constexpr int relaxed_spin_interval = 100;
    for (int cnt = 0; !done.load(std::memory_order_acquire); cnt += (cnt < busy_spin_cnt), std::this_thread::sleep_for(std::chrono::milliseconds(cnt < busy_spin_cnt ? busy_spin_interval : relaxed_spin_interval))) {
        const auto prog_new = prog.load(std::memory_order_acquire);
        if (prog_new > prog_prev) {
            std::printf("\rProgress: %d%%", prog_new);
            std::fflush(stdout);
            prog_prev = prog_new;
        }
    }

    worker.join();

    std::putchar('\r');
    switch (ret) {
    case(MergeResult::Success):
        std::printf("Merge done: %s\n", output);
        break;
    case(MergeResult::InvalidInput):
        std::puts("Merge error: Invalid input file.");
        break;
    case(MergeResult::IoError):
        std::puts("Merge error: Could not open file.");
        break;
    case(MergeResult::InternalError):
        std::puts("Merge error: Internal merge error.");
        break;
    }

    return ret == MergeResult::Success ? 0 : 1;
}
