from os.path import exists, isabs, isdir, join, normpath, splitext
from shutil import copytree, rmtree, which
import os
import subprocess

Import("env")

project_dir = env.subst("$PROJECT_DIR")
local_data_dir = join(project_dir, "data_stopwatch_local")
default_data_dir = join(project_dir, "data_stopwatch")
generated_data_dir = join(project_dir, ".pio", "generated_data_stopwatch")

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

dizzy_png_source_dirs = (
    join(source_data_dir, "guruguru_frames_new_tight_386"),
)
dizzy_png_frames = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
dizzy_png_source_dir = next(
    (
        candidate
        for candidate in dizzy_png_source_dirs
        if all(exists(join(candidate, f"frame_{frame:02d}.png")) for frame in dizzy_png_frames)
    ),
    None,
)

dizzy_sources = (
    (
        "guruguru_new_front25_tight_416_1_8_twice_then_shake_6fps.webp",
        "dizzy_cw_%02d.jpg",
        "dizzy_cw_meta.txt",
    ),
    (
        "guruguru_new_front25_tight_416_reverse_1_8_twice_then_shake_6fps.webp",
        "dizzy_ccw_%02d.jpg",
        "dizzy_ccw_meta.txt",
    ),
)

magick = which("magick")
if magick is None and exists("/opt/homebrew/bin/magick"):
    magick = "/opt/homebrew/bin/magick"
has_dizzy_sources = all(exists(join(source_data_dir, name)) for name, _, _ in dizzy_sources)

def remove_macos_metadata(root_dir):
    for root, _, files in os.walk(root_dir):
        for name in files:
            if name == ".DS_Store":
                os.remove(join(root, name))

if dizzy_png_source_dir and magick:
    if isdir(generated_data_dir):
        rmtree(generated_data_dir)
    copytree(source_data_dir, generated_data_dir)
    remove_macos_metadata(generated_data_dir)

    for name in os.listdir(generated_data_dir):
        path = join(generated_data_dir, name)
        if not os.path.isfile(path):
            continue
        stem, ext = splitext(name)
        ext = ext.lower()
        if ext == ".png":
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
        elif ext in (".webp", ".gif", ".mp4", ".webm"):
            os.remove(path)

    copied_dizzy_png_dir = join(generated_data_dir, "guruguru_frames_new_tight_386")
    if isdir(copied_dizzy_png_dir):
        rmtree(copied_dizzy_png_dir)

    for frame in dizzy_png_frames:
        subprocess.run(
            [
                magick,
                join(dizzy_png_source_dir, f"frame_{frame:02d}.png"),
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
                join(generated_data_dir, f"dizzy_{frame:02d}.jpg"),
            ],
            check=True,
        )

    env.Replace(PROJECT_DATA_DIR=generated_data_dir)
elif has_dizzy_sources and magick:
    if isdir(generated_data_dir):
        rmtree(generated_data_dir)
    copytree(source_data_dir, generated_data_dir)
    remove_macos_metadata(generated_data_dir)

    for name in os.listdir(generated_data_dir):
        path = join(generated_data_dir, name)
        if not os.path.isfile(path):
            continue
        stem, ext = splitext(name)
        ext = ext.lower()
        if ext == ".png":
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
        elif ext in (".webp", ".gif", ".mp4", ".webm"):
            os.remove(path)

    dizzy_work_dir = join(generated_data_dir, "_dizzy_work")
    if isdir(dizzy_work_dir):
        rmtree(dizzy_work_dir)
    os.makedirs(dizzy_work_dir, exist_ok=True)

    for source_name, output_pattern, meta_name in dizzy_sources:
        frame_pattern = join(dizzy_work_dir, splitext(source_name)[0] + "_%02d.png")
        subprocess.run(
            [
                magick,
                join(source_data_dir, source_name),
                "-coalesce",
                "-resize",
                "386x386",
                "-background",
                "black",
                "-alpha",
                "remove",
                "-alpha",
                "off",
                frame_pattern,
            ],
            check=True,
        )

        meta_lines = []
        for frame in range(24):
            frame_path = frame_pattern % frame
            crop_path = join(dizzy_work_dir, f"crop_{frame:02d}.png")
            subprocess.run(
                [
                    magick,
                    frame_path,
                    "-fuzz",
                    "1%",
                    "-trim",
                    crop_path,
                ],
                check=True,
            )
            info = subprocess.check_output(
                [
                    magick,
                    crop_path,
                    "-format",
                    "%X %Y %w %h",
                    "info:",
                ],
                text=True,
            ).strip()
            x_text, y_text, w_text, h_text = info.split()
            x = int(x_text)
            y = int(y_text)
            w = int(w_text)
            h = int(h_text)
            meta_lines.append(f"{x} {y} {w} {h}\n")
            subprocess.run(
                [
                    magick,
                    crop_path,
                    "-sampling-factor",
                    "4:4:4",
                    "-quality",
                    "70",
                    "-strip",
                    join(generated_data_dir, output_pattern % frame),
                ],
                check=True,
            )

        with open(join(generated_data_dir, meta_name), "w", encoding="ascii") as meta_file:
            meta_file.writelines(meta_lines)

    rmtree(dizzy_work_dir)
    env.Replace(PROJECT_DATA_DIR=generated_data_dir)
elif has_dizzy_sources:
    print("[data] magick not found; stopwatch JPEG assets were not generated")
    env.Replace(PROJECT_DATA_DIR=source_data_dir)
else:
    env.Replace(PROJECT_DATA_DIR=source_data_dir)
