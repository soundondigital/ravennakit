#!/usr/bin/env python3 -u
import argparse
import multiprocessing
import os
import platform
import subprocess
from pathlib import Path

from submodules.build_tools.cmake import Config, CMake

test_report_folder = Path('test-reports')
test_report_file = Path('ravennakit_test_report.xml')
ravennakit_tests_target = 'ravennakit_tests'

# Script location matters, cwd does not
script_path = Path(__file__)
script_dir = script_path.parent


def build_macos(args, build_config: Config, subfolder: str, spdlog: bool = False, asan: bool = False,
                tsan: bool = False):
    path_to_build = Path(args.path_to_build) / subfolder

    if args.skip_build:
        return path_to_build

    path_to_build.mkdir(parents=True, exist_ok=True)

    cmake = CMake()
    cmake.path_to_build(path_to_build)
    cmake.path_to_source(script_dir)
    cmake.build_config(build_config)
    cmake.generator('Xcode')
    cmake.parallel(multiprocessing.cpu_count())

    cmake.option('CMAKE_TOOLCHAIN_FILE', 'submodules/vcpkg/scripts/buildsystems/vcpkg.cmake')
    cmake.option('VCPKG_OVERLAY_TRIPLETS', 'triplets')
    cmake.option('VCPKG_TARGET_TRIPLET', 'macos-universal')
    cmake.option('CMAKE_OSX_ARCHITECTURES', 'x86_64;arm64')
    cmake.option('CMAKE_OSX_DEPLOYMENT_TARGET', args.macos_deployment_target)
    cmake.option('CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY', 'Apple Development')
    cmake.option('CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM', args.macos_development_team)
    cmake.option('BUILD_NUMBER', args.build_number)

    # Crash the program when an assertion gets hit
    cmake.option('RAV_ABORT_ON_ASSERT', 'ON')

    if spdlog:
        cmake.option('RAV_ENABLE_SPDLOG', 'ON')
    if asan:
        cmake.option('WITH_ADDRESS_SANITIZER', 'ON')
    if tsan:
        cmake.option('WITH_THREAD_SANITIZER', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build_windows(args, arch, build_config: Config):
    path_to_build = Path(args.path_to_build) / ('windows-' + arch)

    if args.skip_build:
        return path_to_build

    path_to_build.mkdir(parents=True, exist_ok=True)

    cmake = CMake()
    cmake.path_to_build(path_to_build)
    cmake.path_to_source(script_dir)
    cmake.build_config(build_config)
    cmake.architecture(arch)
    cmake.parallel(multiprocessing.cpu_count())

    cmake.option('CMAKE_TOOLCHAIN_FILE', 'submodules/vcpkg/scripts/buildsystems/vcpkg.cmake')
    cmake.option('VCPKG_OVERLAY_TRIPLETS', 'triplets')
    cmake.option('VCPKG_TARGET_TRIPLET', 'windows-' + arch)
    cmake.option('BUILD_NUMBER', args.build_number)
    cmake.option('RAV_ENABLE_SPDLOG', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build_linux(args, arch, build_config: Config):
    path_to_build = Path(args.path_to_build) / ('linux-' + arch)

    if args.skip_build:
        return path_to_build

    path_to_build.mkdir(parents=True, exist_ok=True)

    cmake = CMake()
    cmake.path_to_build(path_to_build)
    cmake.path_to_source(script_dir)
    cmake.build_config(build_config)
    cmake.parallel(multiprocessing.cpu_count())

    cmake.option('CMAKE_TOOLCHAIN_FILE', 'submodules/vcpkg/scripts/buildsystems/vcpkg.cmake')
    cmake.option('VCPKG_TARGET_TRIPLET', arch + '-linux')
    cmake.option('BUILD_NUMBER', args.build_number)
    cmake.option('RAV_ENABLE_SPDLOG', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build(args):
    build_config = Config.debug if args.debug else Config.release_with_debug_info

    test_report_folder.mkdir(parents=True, exist_ok=True)

    def run_test(test_target, report_name):
        subprocess.run([test_target, '--reporter',
                        f'JUnit::out={test_report_folder}/{report_name}.xml', '--reporter',
                        'console::out=-::colour-mode=ansi'],
                       check=True)

    if platform.system() == 'Darwin':
        path_to_build = build_macos(args, build_config, 'macos_universal')
        run_test(path_to_build / build_config.value / ravennakit_tests_target, 'macos_universal')

        path_to_build = build_macos(args, build_config, 'macos_universal_spdlog_asan', spdlog=True, asan=True)
        run_test(path_to_build / build_config.value / ravennakit_tests_target, 'macos_universal_spdlog_asan')

        path_to_build = build_macos(args, build_config, 'macos_universal_spdlog_tsan', spdlog=True, tsan=True)
        run_test(path_to_build / build_config.value / ravennakit_tests_target, 'macos_universal_spdlog_tsan')

    elif platform.system() == 'Windows':
        path_to_build_x64 = build_windows(args, 'x64', build_config)
        run_test(path_to_build_x64 / build_config.value / f'{ravennakit_tests_target}.exe', 'windows_x64')

    elif platform.system() == 'Linux':
        path_to_build_x64 = build_linux(args, 'x64', build_config)
        run_test(path_to_build_x64 / f'{ravennakit_tests_target}', 'linux_x64')

        # TODO: path_to_build_arm64 = build_linux(args, 'arm64', build_config)

def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--debug",
                        help="Enable debug builds",
                        action='store_true')

    parser.add_argument("--path-to-build",
                        help="The folder to build the project in",
                        default="build")

    parser.add_argument("--build-number",
                        help="Specifies the build number",
                        default="0")

    parser.add_argument("--skip-build",
                        help="Skip building",
                        action='store_true')

    parser.add_argument("--upload",
                        help="Upload the archive to spaces",
                        action='store_true')

    parser.add_argument("--spaces-key",
                        help="Specify the key for uploading to spaces")

    parser.add_argument("--spaces-secret",
                        help="Specify the secret for uploading to spaces")

    if platform.system() == 'Darwin':
        parser.add_argument("--notarize",
                            help="Notarize packages",
                            action="store_true")

        parser.add_argument("--macos-deployment-target",
                            help="Specify the minimum macOS deployment target (macOS only)",
                            default="10.13")

        parser.add_argument("--macos-developer-id-application",
                            help="Specify the developer id application identity (macOS only)",
                            default="Developer ID Application: Owllab (3Y6DLW6UXM)")

        parser.add_argument("--macos-developer-id-installer",
                            help="Specify the developer id installer identity (macOS only)",
                            default="Developer ID Installer: Owllab (3Y6DLW6UXM)")

        parser.add_argument("--macos-development-team",
                            help="Specify the Apple development team (macOS only)",
                            default="3Y6DLW6UXM")  # Owllab

        parser.add_argument("--macos-notarization-bundle-id",
                            help="Specify the bundle id for notarization (macOS only)",
                            default="com.owllad.RavennaSDK")

        parser.add_argument("--macos-notarization-username",
                            help="Specify the username for notarization (macOS only)",
                            default="ruurd@owllab.nl")

        parser.add_argument("--macos-notarization-password",
                            help="Specify the username for notarization (macOS only)")

    elif platform.system() == 'Windows':
        parser.add_argument("--sign",
                            help="Sign the installer",
                            action="store_true")

        parser.add_argument("--windows-code-sign-identity",
                            help="Specify the code signing identity (Windows only)",
                            default="431e889eeb203c2db5dd01da91d56186b20d1880")  # GlobalSign cert

    build(parser.parse_args())


if __name__ == '__main__':
    print("Invoke {} as script. Script dir: {}".format(script_path, script_dir))
    main()
