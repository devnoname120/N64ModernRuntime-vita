#include "files.hpp"

constexpr std::u8string_view backup_suffix = u8".bak";
constexpr std::u8string_view temp_suffix = u8".temp";

#ifdef __vita__
// std::filesystem::copy_file also copies host permission metadata. Vita3K does
// not implement the resulting sceIoChstatByFd call; copy the save bytes instead.
static void copy_save_contents(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec) {
    std::ifstream input(from, std::ios::binary);
    if (!input) { ec = std::make_error_code(std::errc::io_error); return; }
    std::ofstream output(to, std::ios::binary | std::ios::trunc);
    if (!output) { ec = std::make_error_code(std::errc::io_error); return; }
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount()) {
        output.write(buffer, input.gcount());
        if (!output) break;
    }
    const bool read_ok = input.eof();
    output.close();
    if (!read_ok || !output) ec = std::make_error_code(std::errc::io_error);
    else ec.clear();
}
#endif

std::ifstream recomp::open_input_backup_file(const std::filesystem::path& filepath, std::ios_base::openmode mode) {
    std::filesystem::path backup_path{filepath};
    backup_path += backup_suffix;
    return std::ifstream{backup_path, mode};
}

std::ifstream recomp::open_input_file_with_backup(const std::filesystem::path& filepath, std::ios_base::openmode mode) {
    std::ifstream ret{filepath, mode};

    // Check if the file failed to open and open the corresponding backup file instead if so.
    if (!ret.good()) {
        return open_input_backup_file(filepath, mode);
    }

    return ret;
}

std::ofstream recomp::open_output_file_with_backup(const std::filesystem::path& filepath, std::ios_base::openmode mode) {
    std::filesystem::path temp_path{filepath};
    temp_path += temp_suffix;
    std::ofstream temp_file_out{ temp_path, mode };

    return temp_file_out;
}

bool recomp::finalize_output_file_with_backup(const std::filesystem::path& filepath) {
    std::filesystem::path backup_path{filepath};
    backup_path += backup_suffix;

    std::filesystem::path temp_path{filepath};
    temp_path += temp_suffix;

    std::error_code ec;
    if (std::filesystem::exists(filepath, ec)) {
#ifdef __vita__
        copy_save_contents(filepath, backup_path, ec);
#else
        std::filesystem::copy_file(filepath, backup_path, std::filesystem::copy_options::overwrite_existing, ec);
#endif
        if (ec) {
            return false;
        }
    }
#ifdef __vita__
    copy_save_contents(temp_path, filepath, ec);
#else
    std::filesystem::copy_file(temp_path, filepath, std::filesystem::copy_options::overwrite_existing, ec);
#endif
    if (ec) {
        return false;
    }
    std::filesystem::remove(temp_path, ec);
    return true;
}
