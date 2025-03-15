# Script location matters, cwd does not
import subprocess
from pathlib import Path

import pygit2

script_path = Path(__file__)
script_dir = script_path.parent

# Git version
repo = pygit2.Repository(path='.')
git_version = repo.describe(pattern='v*')
git_branch = repo.head.shorthand


def replace_in_file(source_file, destination_file, old_string, new_string):
    with open(source_file, "r", encoding="utf-8") as file:
        content = file.read()

    # Replace occurrences of the old string with the new string
    content = content.replace(old_string, new_string)

    # Write the modified content to the destination file
    with open(destination_file, "w", encoding="utf-8") as file:
        file.write(content)

    print(f"Replaced '{old_string}' with '{new_string}' and saved to {destination_file}")


def doxygen_docs():
    replace_in_file(script_dir / 'assets' / 'footer.html.template',
                    script_dir / 'assets' / 'footer.html',
                    '%CUSTOM_FOOTER_TEXT%', f'RAVENNAKIT version {git_version}')
    # Generate html docs
    subprocess.run(['doxygen', 'Doxyfile'], cwd=script_dir, check=True)


if __name__ == '__main__':
    print("Invoke {} as script. Script dir: {}".format(script_path, script_dir))
    doxygen_docs()
