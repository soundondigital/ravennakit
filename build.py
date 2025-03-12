#!/usr/bin/env python3 -u
import argparse
import json
import multiprocessing
import os
import platform
import re
import shutil
import subprocess
from datetime import datetime
from pathlib import Path

import boto3
import pygit2

from docs import generate
from submodules.build_tools.cmake import Config, CMake

test_report_folder = Path('test-reports')
test_report_file = Path('ravennakit_test_report.xml')
ravennakit_tests_target = 'ravennakit_tests'

# Script location matters, cwd does not
script_path = Path(__file__)
script_dir = script_path.parent

# Git version
repo = pygit2.Repository(path='.')
git_version = repo.describe(pattern='v*')
git_branch = repo.head.shorthand


def upload_to_spaces(args, file: Path):
    session = boto3.session.Session()

    key = args.spaces_key
    secret = args.spaces_secret

    if not key:
        raise Exception('Need spaces key')

    if not secret:
        raise Exception('Need spaces secret')

    client = session.client('s3',
                            endpoint_url="https://ams3.digitaloceanspaces.com",
                            region_name="ams3",
                            aws_access_key_id=key,
                            aws_secret_access_key=secret)

    folder = 'branches/' + git_branch

    if git_branch == 'HEAD':
        # If we're in head, we're most likely building from a tag in which case we want to archive the artifacts
        folder = 'archive/' + git_version

    bucket = 'ravennakit'
    file_name = folder + '/' + file.name
    client.upload_file(str(file), bucket, file_name)

    print("Uploaded artefacts to {}/{}".format(bucket, file_name))


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
    cmake.generator('Ninja')
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
        cmake.option('RAV_WITH_ADDRESS_SANITIZER', 'ON')
    if tsan:
        cmake.option('RAV_WITH_THREAD_SANITIZER', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build_windows(args, arch, build_config: Config, subfolder: str, spdlog: bool = False):
    path_to_build = Path(args.path_to_build) / subfolder

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
    cmake.option('RAV_WINDOWS_VERSION', args.windows_version)

    if spdlog:
        cmake.option('RAV_ENABLE_SPDLOG', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build_linux(args, arch, build_config: Config, subfolder: str, spdlog: bool = False):
    path_to_build = Path(args.path_to_build) / subfolder

    if args.skip_build:
        return path_to_build

    path_to_build.mkdir(parents=True, exist_ok=True)

    cmake = CMake()
    cmake.path_to_build(path_to_build)
    cmake.path_to_source(script_dir)
    cmake.build_config(build_config)
    cmake.generator('Ninja')
    cmake.parallel(multiprocessing.cpu_count())

    cmake.option('CMAKE_TOOLCHAIN_FILE', 'submodules/vcpkg/scripts/buildsystems/vcpkg.cmake')
    cmake.option('VCPKG_TARGET_TRIPLET', arch + '-linux')
    cmake.option('BUILD_NUMBER', args.build_number)

    if spdlog:
        cmake.option('RAV_ENABLE_SPDLOG', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build_android(args, arch, build_config: Config, subfolder: str, spdlog: bool = False):
    path_to_build = Path(args.path_to_build) / subfolder

    if args.skip_build:
        return path_to_build

    android_abi_vcpkg_triplets = {"arm64-v8a": "arm64-android", "armeabi-v7a": "arm-android", "x86_64": "x64-android",
                                  "x86": "x86-android"}

    triplet = android_abi_vcpkg_triplets[arch]

    if triplet is None:
        raise ValueError(f'Unknown android architecture {arch}')

    android_ndk_home = Path.home() / 'Library' / 'Android' / 'sdk' / 'ndk' / '23.1.7779620'
    env = os.environ.copy()
    env['ANDROID_NDK_HOME'] = str(android_ndk_home)

    path_to_build.mkdir(parents=True, exist_ok=True)

    cmake = CMake()
    cmake.env(env)
    cmake.path_to_build(path_to_build)
    cmake.path_to_source(script_dir)
    cmake.build_config(build_config)
    cmake.parallel(multiprocessing.cpu_count())

    cmake.option('CMAKE_TOOLCHAIN_FILE', 'submodules/vcpkg/scripts/buildsystems/vcpkg.cmake')
    cmake.option('VCPKG_CHAINLOAD_TOOLCHAIN_FILE', f'{android_ndk_home}/build/cmake/android.toolchain.cmake')
    cmake.option('VCPKG_TARGET_TRIPLET', triplet)
    cmake.option('ANDROID_ABI', arch)
    cmake.option('ANDROID_PLATFORM', 'android-21')
    cmake.option('BUILD_NUMBER', args.build_number)

    if spdlog:
        cmake.option('RAV_ENABLE_SPDLOG', 'ON')

    cmake.configure()
    cmake.build()

    return path_to_build


def build_dist(args):
    path_to_dist = Path(args.path_to_build) / 'dist'
    path_to_dist.mkdir(parents=True, exist_ok=True)

    # Generate html docs
    subprocess.run(['doxygen', 'Doxyfile'], cwd=Path('docs'), check=True)

    generate.doxygen_docs()

    # Manually choose the files to copy to prevent accidental leaking of files when the repo changes or is not clean.

    shutil.copytree('cmake', path_to_dist / 'cmake', dirs_exist_ok=True)
    shutil.copytree('docs', path_to_dist / 'docs', dirs_exist_ok=True)
    shutil.copytree('examples', path_to_dist / 'examples', dirs_exist_ok=True)
    shutil.copytree('include', path_to_dist / 'include', dirs_exist_ok=True)
    shutil.copytree('src', path_to_dist / 'src', dirs_exist_ok=True)
    shutil.copytree('test', path_to_dist / 'test', dirs_exist_ok=True)
    shutil.copytree('triplets', path_to_dist / 'triplets', dirs_exist_ok=True)
    shutil.copytree('submodules/vcpkg', path_to_dist / 'submodules/vcpkg', dirs_exist_ok=True)
    shutil.copy2('.clang-format', path_to_dist)
    shutil.copy2('.gitignore', path_to_dist)
    shutil.copy2('CMakeLists.txt', path_to_dist)
    shutil.copy2('LICENSE.md', path_to_dist)
    shutil.copy2('README.md', path_to_dist)
    shutil.copy2('vcpkg.json', path_to_dist)

    version_data = {
        "version": git_version,
        "build_number": args.build_number,
        "date": str(datetime.now())
    }

    with open(path_to_dist / 'version.json', 'w') as file:
        json.dump(version_data, file, indent=4)

    match = re.match(r"^v([0-9]+)\.([0-9]+)\.([0-9]+)(.*)$", git_version)

    if not match:
        raise ValueError(f'Invalid version {git_version}')

    with open(path_to_dist / 'cmake/version.cmake', 'w') as file:
        file.write(f'set(GIT_DESCRIBE_VERSION "{git_version}")\n')
        file.write(f'set(GIT_VERSION_MAJOR {match.group(1)})\n')
        file.write(f'set(GIT_VERSION_MINOR {match.group(2)})\n')
        file.write(f'set(GIT_VERSION_PATCH {match.group(3)})\n')
        file.write(f'set(BUILD_NUMBER {args.build_number})\n')

    # Create docs zip
    archive_path = args.path_to_build + '/ravennakit-' + git_version + '-' + args.build_number + '-docs'
    zip_path = Path(archive_path + '.zip')
    zip_path.unlink(missing_ok=True)
    shutil.make_archive(archive_path, 'zip', path_to_dist / 'docs' / 'html')

    # Create dist zip
    archive_path = args.path_to_build + '/ravennakit-' + git_version + '-' + args.build_number + '-dist'
    zip_path = Path(archive_path + '.zip')
    zip_path.unlink(missing_ok=True)
    shutil.make_archive(archive_path, 'zip', path_to_dist)

    return zip_path # Only returning the dist package since that is the one we want to upload


def build(args):
    build_config = Config.debug if args.debug else Config.release_with_debug_info

    test_report_folder.mkdir(parents=True, exist_ok=True)

    archive = None

    def run_test(test_target, report_name):
        print(f'Running test {report_name} ({test_target})')

        cmd = [test_target, '--reporter',
               f'JUnit::out={test_report_folder}/{report_name}.xml', '--reporter',
               'console::out=-::colour-mode=ansi']
        subprocess.run(cmd, check=True)

        if platform.system() == 'Darwin' and platform.processor() == 'arm':
            print(f'Running x86_64 test {report_name} ({test_target})')
            subprocess.run(['arch', '--x86_64'] + cmd, check=True)

    if platform.system() == 'Darwin':
        if args.dist:
            archive = build_dist(args)
        elif args.android:
            path_to_build = build_android(args, 'arm64-v8a', build_config, 'android_arm64')
            path_to_build = build_android(args, 'arm64-v8a', build_config, 'android_arm64_spdlog', spdlog=True)

            path_to_build = build_android(args, 'x86_64', build_config, 'android_x64')
            path_to_build = build_android(args, 'x86_64', build_config, 'android_x64_spdlog', spdlog=True)

            path_to_build = build_android(args, 'armeabi-v7a', build_config, 'android_arm')
            path_to_build = build_android(args, 'armeabi-v7a', build_config, 'android_arm_spdlog', spdlog=True)

            path_to_build = build_android(args, 'x86', build_config, 'android_x86')
            path_to_build = build_android(args, 'x86', build_config, 'android_x86_spdlog', spdlog=True)
        else:
            path_to_build = build_macos(args, build_config, 'macos_universal')
            run_test(path_to_build / ravennakit_tests_target, 'macos_universal')

            if args.asan:
                path_to_build = build_macos(args, build_config, 'macos_universal_spdlog_asan', spdlog=True, asan=True)
                run_test(path_to_build / ravennakit_tests_target, 'macos_universal_spdlog_asan')

            if args.tsan:
                path_to_build = build_macos(args, build_config, 'macos_universal_spdlog_tsan', spdlog=True, tsan=True)
                run_test(path_to_build / ravennakit_tests_target, 'macos_universal_spdlog_tsan')

    elif platform.system() == 'Windows':
        path_to_build = build_windows(args, 'x64', build_config, 'windows_x64')
        run_test(path_to_build / build_config.value / f'{ravennakit_tests_target}.exe', 'windows_x64')

        path_to_build = build_windows(args, 'x64', build_config, 'windows_x64_spdlog', spdlog=True)
        run_test(path_to_build / build_config.value / f'{ravennakit_tests_target}.exe', 'windows_x64_spdlog')

    elif platform.system() == 'Linux':
        path_to_build = build_linux(args, 'x64', build_config, 'linux_x64')
        run_test(path_to_build / f'{ravennakit_tests_target}', 'linux_x64')

        path_to_build = build_linux(args, 'x64', build_config, 'linux_x64_spdlog', spdlog=True)
        run_test(path_to_build / f'{ravennakit_tests_target}', 'linux_x64_spdlog')

        # TODO: path_to_build_arm64 = build_linux(args, 'arm64', build_config)

    if archive and args.upload:
        upload_to_spaces(args, archive)


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--debug",
                        help="Enable debug builds",
                        action='store_true')

    parser.add_argument("--path-to-build",
                        help="The folder to build the project in",
                        default="build")

    parser.add_argument("--dist",
                        help="Prepare the source code for distribution",
                        action='store_true')

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
        parser.add_argument("--android",
                            help="Build for Android, requires Android NDK.",
                            action="store_true")

        parser.add_argument("--asan",
                            help="Build and run with address sanitizer (separately)",
                            action="store_true")

        parser.add_argument("--tsan",
                            help="Build and run with thread sanitizer (separately)",
                            action="store_true")

        parser.add_argument("--notarize",
                            help="Notarize packages",
                            action="store_true")

        parser.add_argument("--macos-deployment-target",
                            help="Specify the minimum macOS deployment target (macOS only)",
                            default="10.15")  # Catalina

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

        parser.add_argument("--windows-version",
                            help="Specify the minimum supported version of Windows",
                            default="0x0A00") # Windows 10

    build(parser.parse_args())


if __name__ == '__main__':
    print("Invoke {} as script. Script dir: {}".format(script_path, script_dir))
    main()
