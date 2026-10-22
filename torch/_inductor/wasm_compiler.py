"""Compile Inductor C++ through published Clang/LLD libraries in this process."""
import ctypes
import functools
import json
import shlex
import sys
import time
from pathlib import Path

ROOT = Path(__file__).with_name("wasm")
SYSROOT = ROOT / "sysroot"
POINTER_BITS = ctypes.sizeof(ctypes.c_void_p) * 8
builds = []


@functools.cache
def _compiler():
    library = ctypes.CDLL(str(ROOT / "torch_inductor_compiler.so"))
    library.torch_inductor_clang_version.restype = ctypes.c_char_p
    for name in ("torch_inductor_clang_compile", "torch_inductor_lld_link"):
        entry = getattr(library, name)
        entry.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
        entry.restype = ctypes.c_int
    return library


def version():
    return _compiler().torch_inductor_clang_version().decode()


def _call(name, arguments):
    argv = (ctypes.c_char_p * len(arguments))(*(arg.encode() for arg in arguments))
    result = getattr(_compiler(), name)(len(arguments), argv)
    if result:
        raise RuntimeError(f"{name} failed ({result}): {shlex.join(arguments)}")


def build(builder):
    target = Path(builder.get_target_file_path())
    target.parent.mkdir(parents=True, exist_ok=True)
    tokens = shlex.split(builder.get_command_line())
    sources = [arg for arg in tokens if Path(arg).suffix in (".cpp", ".cc", ".cxx")]
    if len(sources) != 1:
        raise RuntimeError(f"Expected one Inductor C++ source, got {sources}")
    source = sources[0]
    obj = target if target.suffix == ".o" else target.with_suffix(".o")
    arguments = ["clang++", f"--target=wasm{POINTER_BITS}-unknown-emscripten",
        f"--sysroot={SYSROOT}", f"-resource-dir={SYSROOT}/lib/clang/23",
        "-isystem", str(SYSROOT / "include/c++/v1"),
        "-isystem", str(SYSROOT / "include/compat"),
        "-I" + str(SYSROOT / f"include/python{sys.version_info.major}.{sys.version_info.minor}")]
    skip = False
    for token in tokens[1:]:
        if skip:
            skip = False
            continue
        if token == "-o":
            skip = True
        elif token == source or token == "-shared" or token.startswith(("-l", "-L", "-Wl,")):
            continue
        else:
            arguments.append(token)
    arguments += ["-fPIC", "-fwasm-exceptions", "-msimd128", "-mno-tail-call",
                  "-c", source, "-o", str(obj)]
    started = time.perf_counter()
    _call("torch_inductor_clang_compile", arguments)
    compiled = time.perf_counter()
    link_args = None
    if target != obj:
        link_args = ["wasm-ld", f"-mwasm{POINTER_BITS}", "--shared", "--import-memory",
            "--experimental-pic", "--unresolved-symbols=import-dynamic",
            "--export-dynamic", "-Bsymbolic", str(obj), "-o", str(target)]
        _call("torch_inductor_lld_link", link_args)
    record = {"source": source, "output": str(target), "clang": arguments,
              "lld": link_args, "compile_seconds": compiled - started,
              "link_seconds": time.perf_counter() - compiled}
    builds.append(record)
    target.with_suffix(".build.json").write_text(json.dumps(record, indent=2))
