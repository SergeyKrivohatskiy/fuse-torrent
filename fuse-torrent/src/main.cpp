#include "download_torrent.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>


int main(int argc, char *argv[])
{
    CLI::App app(
            "A minimal torrent client "
            "that, in addition to just downloading a torrent, allows "
            "using any file from the torrent before fully downloading "
            "via a virtual file system",
            "FUSE torrent client");

    std::filesystem::path torrentFile;
    app.add_option("torrent_file", torrentFile, "'.torrent' file to download")
            ->required()
            ->check(CLI::ExistingFile);

    std::filesystem::path targetDirectory;
    app.add_option(
            "target_diretory", targetDirectory,
            "directory where torrent files will be downloaded to")
                    ->required();

    std::filesystem::path mappingDirectory;
    app.add_option(
            "mapping_diretory", mappingDirectory,
            "a directory where a virtual file system will be mounted")
                    ->required()
                    ->check(CLI::NonexistentPath);

    bool clearTargetDirectory = false;
    app.add_flag("--clear", clearTargetDirectory, "crear target directory");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (clearTargetDirectory) {
        std::filesystem::remove_all(targetDirectory);
    } else {
        if (std::filesystem::exists(targetDirectory)) {
            std::cerr << "target_directory\"" << targetDirectory << "\" should not exist";
            return -1;
        }
    }

    downloadTorrent(torrentFile, targetDirectory, mappingDirectory);
    
    return 0;
}
