#include "slate_utils.hpp"
#include <slate/mp4/mp4_merge.hpp>
#include "version.hpp"
#include <print>
#include <cstdio>
#include "string_utils.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>
#include <array>
#include "sys_utils.hpp"

namespace fs = std::filesystem;

namespace {

bool get_yn(bool default_val = false) noexcept
{
    const auto ans = std::getchar();
    if (default_val == false) {
        return ans == 'y' || ans == 'Y';
    }
    return ans != 'n' && ans != 'N';
}

auto get_spinner() noexcept
{
    // Characters for spinning progress indicator.
    static constexpr auto chars = std::to_array<std::string_view>({"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"});
    return [i = 0u] mutable -> auto& {
        const auto& ret = chars[i];
        if (++i == chars.size())
            i = 0;
        return ret;
    };
}


const auto& help_msg =
R"^^(mp4join: Utility for joining consecutive MP4 files.

Usage: mp4join <FILE_1> <FILE_2> [...] [-o OUTPUT]

Options:
  -o, --output <OUTPUT>  Specify output file path.
  -f, --force            Overwrite existing output file without asking for confirmation.
  -V, --version          Print version information and exit.
  -h, --help             Print this help message and exit.
)^^";

} // namespace

int slate::main_mp4join(int argc, char** argv) noexcept
{
    std::vector<const char*> inputs;
    fs::path output;
    bool force_overwrite = false;

    {
        bool print_version = false;
        bool err_flag = 0;
        for (int i = 1; i < argc; ++i) {
            if (match(argv[i], "-o", "--output")) {
                if (++i < argc) output = argv[i];
                else err_flag = 1;
            } else if (match(argv[i], "-f", "--force")) {
                force_overwrite = true;
            } else if (match(argv[i], "-V", "--version")) {
                print_version = true;
                break;
            } else if (match(argv[i], "-h", "--help")) {
                std::fputs(help_msg, stdout);
                return 0;
            } else {
                inputs.emplace_back(argv[i]);
            }
            if (err_flag) break;
        }

        if (print_version) {
            using namespace slate::version;
            print_info();
            return 0;
        }
        if (err_flag || inputs.size() < 2) {
            std::print(stderr, "Invalid arguments.\nPass '-h' for help.\n");
            return 2;
        }
    }

    if (output.empty()) {
        output = inputs.front();
        output.replace_filename(output.stem().concat("_joined") += (output.extension()));
    }
    const auto output_str = output.string();

    const bool stdout_is_tty = sys::is_tty(stdout);

    if (!force_overwrite && fs::exists(output)) {
        if (stdout_is_tty) { // prompt the user
            std::print("Output file {} already exists. Overwrite? [y/N] ", output_str);
            if (!get_yn(false)) {
                std::println("Aborting.");
                return 0;
            }
        } else { // can't ask, refuse to proceed
            std::println(stderr, "Refusing to overwrite existing output file {}.", output_str);
            return 1;
        }
    }

    using namespace slate::mp4;
    MergeResult ret;
    std::atomic<bool> done = false;
    std::atomic<int> prog = -1;
    //int prog_prev = -1;

    const auto prog_fn = [](void* data, int prog_) {
        static_cast<std::atomic<int>*>(data)->store(prog_, std::memory_order_release);
    };

    Mp4Merger merger;
    for (const auto& file : inputs) {
        merger.add_input(file);
    }
    merger.set_output(output_str).set_progress_callback(prog_fn, &prog);

    std::thread worker {
        [&] {
            ret = merger.run();
            done.store(true, std::memory_order_release);
        }
    };

    if (stdout_is_tty) {
        // std::fputs("\x1b[?25l", stdout); // hide cursor
        static constexpr int busy_spin_cnt = 5;
        static constexpr int busy_spin_interval = 1;
        static constexpr int relaxed_spin_interval = 100;
        auto spinner = get_spinner();
        for (int cnt = 0; !done.load(std::memory_order_acquire); cnt += (cnt < busy_spin_cnt), std::this_thread::sleep_for(std::chrono::milliseconds(cnt < busy_spin_cnt ? busy_spin_interval : relaxed_spin_interval))) {
            const auto prog_new = prog.load(std::memory_order_acquire);
            if (prog_new >= 0 && cnt >= busy_spin_cnt - 1) {
                std::print("\r\x1b[36m{}\x1b[0m Progress: \x1b[1m{}%\x1b[0m", spinner(), prog_new);
                std::fflush(stdout);
                // prog_prev = prog_new;
            }
        }
        // Clear the progress line
        std::fputs("\x1b[2K\r", stdout);
        std::fputs("\x1b[2K\r", stderr);
    }

    worker.join();

    using enum MergeResult;
    switch (ret) {
    case(Success):
        std::println("Merge done: {}", output_str);
        break;
    case(InvalidConfig):
        std::println(stderr, "\x1b[1;31mError:\x1b[0m Invalid configuration.");
        break;
    case(InvalidInput):
        std::println(stderr, "\x1b[1;31mError:\x1b[0m Invalid input file.");
        break;
    case(IoError):
        std::println(stderr, "\x1b[1;31mError:\x1b[0m Could not open file.");
        break;
    case(InternalError):
        std::println(stderr, "\x1b[1;31mError:\x1b[0m Internal merge error.");
        break;
    }

    return ret == Success ? 0 : 1;
}
