from os.path import exists, isabs, isdir, join, normpath, splitext
from shutil import copy2, copytree, rmtree, which
import os
import subprocess

Import("env")

project_dir = env.subst("$PROJECT_DIR")
local_data_dir = join(project_dir, "data_local")
default_data_dir = join(project_dir, "data")
generated_data_dir = join(project_dir, ".pio", "generated_data_cores3")

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

direction_17_from_25 = (14, 19, 24, 23, 22, 21, 20, 15, 10, 5, 0, 1, 2, 3, 4, 9, 12)
dizzy_png_frames = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
dizzy_png_source_dirs = (
    join(source_data_dir, "guruguru_frames_new_tight_240"),
    join(source_data_dir, "guruguru_frames_new_tight"),
)
dizzy_png_source_dir = next(
    (
        candidate
        for candidate in dizzy_png_source_dirs
        if all(exists(join(candidate, f"frame_{frame:02d}.png")) for frame in dizzy_png_frames)
    ),
    None,
)

magick = which("magick")
if magick is None and exists("/opt/homebrew/bin/magick"):
    magick = "/opt/homebrew/bin/magick"

if isdir(generated_data_dir):
    rmtree(generated_data_dir)
copytree(source_data_dir, generated_data_dir)

for root, _, files in os.walk(generated_data_dir):
    for name in files:
        if name in (".DS_Store", ".gitkeep"):
            os.remove(join(root, name))

def image_path_for(prefix, index):
    for ext in (".png", ".jpg", ".jpeg"):
        path = join(source_data_dir, f"{prefix}{index}{ext}")
        if exists(path):
            return path
    return None

def image_path_for_stem(stem):
    for ext in (".png", ".jpg", ".jpeg"):
        path = join(source_data_dir, stem + ext)
        if exists(path):
            return path
    return None

def write_mapped_image(source_path, output_path):
    if source_path is None:
        return
    if magick:
        subprocess.run(
            [
                magick,
                source_path,
                "-resize",
                "240x240!",
                "-background",
                "black",
                "-alpha",
                "remove",
                "-alpha",
                "off",
                "-strip",
                output_path,
            ],
            check=True,
        )
    else:
        copy2(source_path, output_path)

def convert_top_level_pngs_to_jpeg():
    if not magick:
        print("[data] magick not found; CoreS3 top-level PNG assets were not converted to JPEG")
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

for name in os.listdir(generated_data_dir):
    path = join(generated_data_dir, name)
    if not os.path.isfile(path):
        continue

    stem, ext = splitext(name)
    ext = ext.lower()
    if stem.startswith("dir") or stem.startswith("blink"):
        suffix = stem[3:] if stem.startswith("dir") else stem[5:]
        if suffix.isdigit():
            os.remove(path)
            continue
    if stem.startswith("dizzy_cw_") or stem.startswith("dizzy_ccw_") or ext in (".webp", ".gif", ".mp4", ".webm"):
        os.remove(path)

if all(image_path_for_stem(f"dizzy_{frame:02d}") for frame in dizzy_png_frames):
    for frame in dizzy_png_frames:
        write_mapped_image(
            image_path_for_stem(f"dizzy_{frame:02d}"),
            join(generated_data_dir, f"dizzy_{frame:02d}.png"),
        )
elif dizzy_png_source_dir and magick:
    for frame in dizzy_png_frames:
        subprocess.run(
            [
                magick,
                join(dizzy_png_source_dir, f"frame_{frame:02d}.png"),
                "-resize",
                "240x240!",
                "-background",
                "black",
                "-alpha",
                "remove",
                "-alpha",
                "off",
                "-strip",
                join(generated_data_dir, f"dizzy_{frame:02d}.png"),
            ],
            check=True,
        )
elif dizzy_png_source_dir:
    print("[data] magick not found; CoreS3 dizzy PNG frames were not generated")

if all(image_path_for("dir", index) for index in direction_17_from_25):
    for output_index, source_index in enumerate(direction_17_from_25):
        write_mapped_image(
            image_path_for("dir", source_index),
            join(generated_data_dir, f"dir{output_index}.png"),
        )
    if all(image_path_for("blink", index) for index in direction_17_from_25):
        for output_index, source_index in enumerate(direction_17_from_25):
            write_mapped_image(
                image_path_for("blink", source_index),
                join(generated_data_dir, f"blink{output_index}.png"),
            )
elif all(image_path_for("dir", index) for index in range(17)):
    for index in range(17):
        write_mapped_image(
            image_path_for("dir", index),
            join(generated_data_dir, f"dir{index}.png"),
        )
    if all(image_path_for("blink", index) for index in range(17)):
        for index in range(17):
            write_mapped_image(
                image_path_for("blink", index),
                join(generated_data_dir, f"blink{index}.png"),
            )
else:
    print("[data] CoreS3 25-direction face assets missing; guruguru faces were not remapped")

convert_top_level_pngs_to_jpeg()

env.Replace(PROJECT_DATA_DIR=generated_data_dir)
