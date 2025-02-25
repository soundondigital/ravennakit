import argparse
import shutil
import subprocess
from pathlib import Path

import pygit2

# Script location matters, cwd does not
script_path = Path(__file__)
script_dir = script_path.parent

# Git version
repo = pygit2.Repository(path='.')
git_version = repo.describe(pattern='v*')
git_branch = repo.head.shorthand


def commit_push_docs(args):
    repo_path = args.repo_path
    path_to_dist = args.path_to_dist

    if repo_path.exists():
        shutil.rmtree(repo_path)

    subprocess.run(['git', 'clone',
                    f'https://x-token-auth:{args.repo_access_token}@bitbucket.org/soundondigital/ravennakit.com.git',
                    args.repo_path])
    shutil.copytree(path_to_dist / 'docs' / 'html', repo_path / 'docs' / git_version, dirs_exist_ok=True)

    subprocess.run(['git', 'add', '--all'], cwd=repo_path)
    subprocess.run(['git', 'commit', '-a', '-m', f'Add ravennakit docs for version {git_version}'], cwd=repo_path)
    subprocess.run(['git', 'push'], cwd=repo_path)


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--path-to-dist", type=Path, default='build/dist',
                        help="Path to the dist folder")

    parser.add_argument("--repo-path", type=Path, default='ravennakit.com',
                        help="Path to temporarily store the ravennakit.com repository")

    parser.add_argument("--repo-access-token",
                        help="The token for accessing the ravennakit.com repository")

    commit_push_docs(parser.parse_args())


if __name__ == '__main__':
    print("Invoke {} as script. Script dir: {}".format(script_path, script_dir))
    main()
