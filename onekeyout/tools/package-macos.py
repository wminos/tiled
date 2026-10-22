#!/usr/bin/env python3
"""Produce a self-contained local macOS app without altering the Qbs build."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(args, check=True, timeout=120)


def macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as stream:
        return stream.read(4) in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def main():
    if not shutil.which("macdeployqt"):
        raise SystemExit("macdeployqt is required (brew install qttools).")
    source = ROOT / "build/debug/bin/One Key Out.app"
    libraries = list((ROOT / "build/debug").glob("libtiled.*/libtiled.dylib"))
    if not source.is_dir() or len(libraries) != 1:
        raise SystemExit("Build the game first with ./run.sh --build-only.")
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".package-", dir=dist) as temporary:
        bundle = Path(temporary) / "One Key Out.app"
        shutil.copytree(source, bundle, symlinks=True)
        frameworks = bundle / "Contents/Frameworks"
        frameworks.mkdir(parents=True, exist_ok=True)
        library = frameworks / "libtiled.dylib"
        shutil.copy2(libraries[0], library)
        run("install_name_tool", "-id", "@rpath/libtiled.dylib", str(library))
        plugin_source = Path(subprocess.check_output(
            ["qtpaths", "--query", "QT_INSTALL_PLUGINS"], text=True).strip())
        platform_directory = bundle / "Contents/PlugIns/platforms"
        platform_directory.mkdir(parents=True)
        platform_plugins = []
        for name in ("libqcocoa.dylib", "libqoffscreen.dylib"):
            target = platform_directory / name
            shutil.copy2(plugin_source / "platforms" / name, target)
            platform_plugins.append("-executable=" + str(target))
        # The game uses PNG (built into QtGui), not PDF/SVG/QML or virtual keyboards.
        # Explicit plugins avoid deploying unrelated Homebrew Qt modules.
        run("macdeployqt", str(bundle), "-always-overwrite", "-no-codesign", "-no-plugins",
            "-executable=" + str(library), *platform_plugins,
            "-libpath=" + str(libraries[0].parent), "-verbose=1")
        resources = bundle / "Contents/Resources"
        resources.mkdir(parents=True, exist_ok=True)
        (resources / "qt.conf").write_text("[Paths]\nPlugins = PlugIns\n", encoding="utf-8")
        licenses = bundle / "Contents/Resources/licenses"
        licenses.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT.parent / "LICENSE.BSD", licenses / "libtiled-BSD.txt")
        shutil.copy2(ROOT / "THIRD_PARTY.md", licenses / "THIRD_PARTY.md")
        binaries = [path for path in bundle.rglob("*") if macho(path)]
        identities = {}
        replacements = {}
        for path in binaries:
            output = subprocess.check_output(["otool", "-D", str(path)], text=True).splitlines()
            identity = output[1].strip() if len(output) > 1 else None
            identities[path] = identity
            if identity and path.is_relative_to(frameworks):
                replacement = "@rpath/" + path.relative_to(frameworks).as_posix()
                replacements[identity] = replacement
                run("install_name_tool", "-id", replacement, str(path))
        # macdeployqt can leave Homebrew third-party library IDs and references
        # absolute. Rewrite only dependencies whose exact identity was deployed.
        for path in binaries:
            linked = subprocess.check_output(["otool", "-L", str(path)], text=True).splitlines()[1:]
            for line in linked:
                dependency = line.strip().split(" (", 1)[0]
                replacement = replacements.get(dependency)
                if replacement and dependency != identities[path] and dependency != replacement:
                    run("install_name_tool", "-change", dependency, replacement, str(path))
        # Build-tree search paths must not conceal a missing deployed dependency.
        for path in binaries:
            commands = subprocess.check_output(["otool", "-l", str(path)], text=True).splitlines()
            for index, line in enumerate(commands):
                if line.strip() == "cmd LC_RPATH":
                    rpath = commands[index + 2].strip().removeprefix("path ").split(" (offset", 1)[0]
                    if rpath.startswith("/") and not rpath.startswith(("/System/", "/usr/lib/")):
                        run("install_name_tool", "-delete_rpath", rpath, str(path))
            linked = subprocess.check_output(["otool", "-L", str(path)], text=True).splitlines()[1:]
            for line in linked:
                dependency = line.strip().split(" (", 1)[0]
                if dependency == identities[path] and not path.is_relative_to(frameworks):
                    continue  # A plugin's own install name is not a dependency.
                if dependency.startswith("/") and not dependency.startswith(("/System/", "/usr/lib/")):
                    raise SystemExit(f"Undeployed dependency in {path.name}: {dependency}")
                if dependency.startswith("@rpath/") and not (frameworks / dependency[7:]).exists():
                    raise SystemExit(f"Missing bundle framework in {path.name}: {dependency}")
        run("codesign", "--force", "--deep", "--sign", "-", str(bundle))
        run("codesign", "--verify", "--deep", "--strict", str(bundle))
        executable = bundle / "Contents/MacOS/One Key Out"
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith(("DYLD_", "QT_", "QML_"))}
        environment["QT_QPA_PLATFORM"] = "offscreen"
        subprocess.run([str(executable), "--validate"], cwd="/private/tmp", env=environment, check=True, timeout=15)
        destination = dist / "One Key Out.app"
        if destination.exists():
            shutil.rmtree(destination)
        shutil.move(str(bundle), destination)
        print(f"Packaged and verified: {destination}")


if __name__ == "__main__":
    main()
