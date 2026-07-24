Import("env")

from pathlib import Path


developer_home = str(Path.home().resolve())
anonymous_root = "/stackchan-build"
env.Append(
    CCFLAGS=[
        f"-ffile-prefix-map={developer_home}={anonymous_root}",
        f"-fdebug-prefix-map={developer_home}={anonymous_root}",
    ]
)
