import argparse
import glob
import subprocess
from pathlib import Path

import pygit2

# Git version
repo = pygit2.Repository(path='.')
git_version = repo.describe(pattern='v*')
git_branch = repo.head.shorthand

script_path = Path(__file__)
script_dir = script_path.parent


def upload_docs_using_sftp(args):
    files = glob.glob(str(args.path_to_build) + '/*-docs.zip')
    if files is None:
        print("No files found")
        return
    subprocess.run(['scp', '-r', files[0], f'{args.username}@{args.hostname}:{args.remote_path}/docs.zip'], check=True)
    subprocess.run(['ssh', f'{args.username}@{args.hostname}',
                    f'mkdir -p {args.remote_path}/{git_version} && unzip {args.remote_path}/docs.zip -d {args.remote_path}/{git_version} && rm {args.remote_path}/docs.zip'],
                   check=True)


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--path-to-build", type=Path, help="Path to the build folder", default='build')
    parser.add_argument("--hostname", type=str, help="SSH hostname", required=True)
    parser.add_argument("--username", type=str, help="SSH username", required=True)
    parser.add_argument("--remote-path",
                        type=str,
                        help="The path on the remote server to upload the files to",
                        required=True)

    upload_docs_using_sftp(parser.parse_args())


if __name__ == '__main__':
    print("Invoke {} as script. Script dir: {}".format(script_path, script_dir))
    main()
