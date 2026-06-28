from os.path import exists, isabs, isdir, join, normpath
from shutil import copytree, rmtree, which
import subprocess
import os
from os.path import splitext

Import("env")

project_dir = env.subst("$PROJECT_DIR")
local_data_dir = join(project_dir, "data_atoms3r_local")
default_data_dir = join(project_dir, "data_atoms3r")
generated_data_dir = join(project_dir, ".pio", "generated_data_atoms3r")

def selected_data_dir(default_dir):
    requested = os.environ.get("STACKCHAN_FACE_DATA_DIR")
    if not requested:
        requested = env.GetProjectOption("custom_face_data_dir", "")
    requested = requested.strip()
    if not requested:
        print(f"[data] using default face data dir: {default_dir}")
        return default_dir
    selected = requested if isabs(requested) else join(project_dir, requested)
    selected = normpath(selected)
    if not isdir(selected):
        raise RuntimeError(f"selected face data dir does not exist: {selected}")
    print(f"[data] using selected face data dir: {selected}")
    return selected

source_data_dir = selected_data_dir(default_data_dir)

dizzy_source = "guruguru_new_front25_tight_128_1_8_twice_then_shake_6fps.webp"
dizzy_png_frames = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)

magick = which("magick")
if magick is None and exists("/opt/homebrew/bin/magick"):
    magick = "/opt/homebrew/bin/magick"
has_dizzy_png_frames = all(exists(join(source_data_dir, f"dizzy_{frame:02d}.png")) for frame in dizzy_png_frames)
has_dizzy_source = exists(join(source_data_dir, dizzy_source))

def convert_top_level_pngs_to_jpeg():
    if not magick:
        print("[data] magick not found; AtomS3R top-level PNG assets were not converted to JPEG")
        return
    for name in os.listdir(generated_data_dir):
        path = join(generated_data_dir, name)
        if not os.path.isfile(path):
            continue
        stem, ext = splitext(name)
        if ext.lower() != ".png":
            continue
        subprocess.run(
            [
                magick,
                path,
                "-background",
                "black",
                "-alpha",
                "remove",
                "-alpha",
                "off",
                "-sampling-factor",
                "4:4:4",
                "-quality",
                "82",
                "-strip",
                join(generated_data_dir, stem + ".jpg"),
            ],
            check=True,
        )
        os.remove(path)

def remove_macos_metadata():
    for root, _, files in os.walk(generated_data_dir):
        for name in files:
            if name == ".DS_Store":
                os.remove(join(root, name))

if (has_dizzy_png_frames or has_dizzy_source) and magick:
    if isdir(generated_data_dir):
        rmtree(generated_data_dir)
    copytree(source_data_dir, generated_data_dir)
    remove_macos_metadata()

    for name in os.listdir(generated_data_dir):
        if name.startswith("dizzy_cw_") or name.startswith("dizzy_ccw_") or name.endswith(".webp"):
            path = join(generated_data_dir, name)
            if exists(path):
                os.remove(path)

    if not has_dizzy_png_frames:
        dizzy_work_dir = join(generated_data_dir, "_dizzy_work")
        if isdir(dizzy_work_dir):
            rmtree(dizzy_work_dir)
        os.makedirs(dizzy_work_dir, exist_ok=True)

        frame_pattern = join(dizzy_work_dir, "source_%02d.png")
        subprocess.run(
            [
                magick,
                join(source_data_dir, dizzy_source),
                "-coalesce",
                frame_pattern,
            ],
            check=True,
        )

        source_indices = list(range(0, 8)) + list(range(16, 23))
        for output_frame, source_index in enumerate(source_indices, start=1):
            subprocess.run(
                [
                    magick,
                    frame_pattern % source_index,
                    "-background",
                    "black",
                    "-alpha",
                    "remove",
                    "-alpha",
                    "off",
                    "-strip",
                    join(generated_data_dir, f"dizzy_{output_frame:02d}.png"),
                ],
                check=True,
            )

        rmtree(dizzy_work_dir)

    convert_top_level_pngs_to_jpeg()
    env.Replace(PROJECT_DATA_DIR=generated_data_dir)
elif has_dizzy_source:
    print("[data] magick not found; dizzy PNG frames were not generated")
    env.Replace(PROJECT_DATA_DIR=source_data_dir)
else:
    if isdir(generated_data_dir):
        rmtree(generated_data_dir)
    copytree(source_data_dir, generated_data_dir)
    remove_macos_metadata()
    convert_top_level_pngs_to_jpeg()
    env.Replace(PROJECT_DATA_DIR=generated_data_dir)
