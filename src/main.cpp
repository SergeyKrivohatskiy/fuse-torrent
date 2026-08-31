#include "fuse_torrent.hpp"

#include <CLI/CLI.hpp>

#include <exception>
#include <filesystem>
#include <iostream>


int main(int argc, char *argv[])
{
    CLI::App app(
            "A minimal torrent client "
            "that, in addition to just downloading a torrent, allows "
            "using any file from the torrent before fully downloading "
            "via a virtual file system",
            "FuseTorrent");

    std::filesystem::path torrentFile;
    app.add_option("torrent_file", torrentFile, "'.torrent' file to download")
            ->required()
            ->check(CLI::ExistingFile);

    std::filesystem::path targetDirectory;
    app.add_option(
            "target_directory", targetDirectory,
            "directory where torrent files will be downloaded to")
                    ->required();

    std::filesystem::path mappingDirectory;
    app.add_option(
            "mapping_directory", mappingDirectory,
            "a directory where a virtual file system will be mounted")
                    ->required()
                    ->check(CLI::NonexistentPath);

    bool clearTargetDirectory = false;
    app.add_flag("--clear", clearTargetDirectory, "clear the target directory before downloading");

    try {
        app.parse(argc, argv);
    } catch (CLI::ParseError const &e) {
        return app.exit(e);
    }

    try {
        if (clearTargetDirectory) {
            std::filesystem::remove_all(targetDirectory);
        } else if (std::filesystem::exists(targetDirectory)) {
            std::cerr << "target_directory " << targetDirectory
                      << " should not exist\n";
            return 1;
        }

        return downloadTorrentWithFuseMapping(
                torrentFile, targetDirectory, mappingDirectory);
    } catch (std::exception const &e) {
        std::cerr << "FuseTorrent failed: " << e.what() << "\n";
        return 1;
    }
}
