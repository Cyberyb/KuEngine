"""Real GPU regression for the example; uses only Python's standard library.

Every run is finite. Captures are paired by metadata's actual submission and
slot, including the final pending frames; comparisons use linear half floats.
"""
import argparse
import hashlib
import math
import pathlib
import struct
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app", required=True, type=pathlib.Path)
    parser.add_argument("--evidence", required=True, type=pathlib.Path)
    parser.add_argument("--validation", choices=("required", "optional"), default="optional")
    parser.add_argument("--require-validation", dest="validation", action="store_const", const="required")
    args = parser.parse_args()
    args.app = args.app.resolve()
    args.evidence = args.evidence.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    args.evidence = pathlib.Path(tempfile.mkdtemp(prefix="run-", dir=args.evidence))
    print("Volumetric evidence: " + str(args.evidence), flush=True)
    invalid = subprocess.run([str(args.app), "--volumetric-readback-prefix", str(args.evidence / "unbounded")],
                             cwd=args.app.parent, capture_output=True, text=True, timeout=10)
    assert invalid.returncode == 2 and "positive --smoke-frames" in invalid.stderr, "unbounded readback was not rejected before GPU initialization"

    def run(name, extra=(), slots=2, frames=3):
        # Separate invocation prefixes avoid reuse of stale evidence. Verify all
        # expected submit IDs, not glob count or a lone success log marker.
        prefix = args.evidence / name
        command = [str(args.app), "--smoke-frames", str(frames),
                   "--frames-in-flight", str(slots), "--volumetric-time", "1.25",
                   "--volumetric-readback-prefix", str(prefix), *extra]
        if args.validation == "required":
            command.append("--smoke-require-validation")
        result = subprocess.run(command, cwd=args.app.parent, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=90)
        (args.evidence / (name + ".log")).write_text(
            "COMMAND: " + subprocess.list2cmdline(command) + "\n" + result.stdout + result.stderr,
            encoding="utf-8")
        if result.returncode == 77:
            print("KUENGINE_VOLUMETRIC_SKIP " + name)
            sys.exit(77)
        assert result.returncode == 0, (name, result.returncode, result.stdout[-4000:], result.stderr)
        assert "KUENGINE_SMOKE_PASS" in result.stdout, name
        assert "KUENGINE_VALIDATION_ERROR" not in result.stdout + result.stderr, name
        captures = {}
        # Match by submission AND expected rotating slot; duplicate files are an
        # error, so reruns should use the same viewport/configuration or a fresh dir.
        for submit in range(1, frames + 1):
            slot = (submit - 1) % slots
            files = list(args.evidence.glob(f"{name}_submit{submit:06d}_slot{slot}_*.txt"))
            assert len(files) == 1, (name, submit, files)
            path = files[0]
            metadata = dict(line.split("=", 1) for line in path.read_text().splitlines())
            assert int(metadata["submit"]) == submit and int(metadata["slot"]) == slot
            width, height = int(metadata["width"]), int(metadata["height"])
            payload = path.with_suffix(".rgba16f").read_bytes()
            assert len(payload) == width * height * 8, name
            metadata["payloadSha256"] = hashlib.sha256(payload).hexdigest()
            # GPU-side finite capture checks cover every pixel. Independently
            # parse the actual artifact here, including alpha and zero-light case.
            total = 0.0
            for r, g, b, a in struct.iter_unpack("<eeee", payload):
                assert all(math.isfinite(v) and v >= 0 for v in (r, g, b)) and a == 1, name
                total += r + g + b
            assert abs(total - float(metadata["radianceSum"])) <= max(1e-5, total * 2e-8), name
            captures[submit] = metadata
        return captures

    baseline = run("fixed_two_slots")
    assert len({m["payloadSha256"] for m in baseline.values()}) == 1, "fixed time differs between slots"
    single = run("fixed_one_slot", slots=1)
    assert {m["payloadSha256"] for m in single.values()} == {baseline[1]["payloadSha256"]}, "one/two-slot images differ"
    assert float(baseline[1]["radianceSum"]) > 100, "reference frame is empty"
    animation = run("animation", ["--volumetric-time-step", "0.5"])
    assert [float(m["time"]) for m in animation.values()] == [1.25, 1.75, 2.25]
    assert len({m["payloadSha256"] for m in animation.values()}) == 3, "moving light/noise did not change"
    density = run("density", ["--volumetric-density", "0.2"])
    assert density[1]["payloadSha256"] != baseline[1]["payloadSha256"], "density control has no effect"
    improved = run("strong_improved", ["--volumetric-strong-fog", "5", "--volumetric-no-noise"])
    basic = run("strong_basic", ["--volumetric-strong-fog", "5", "--volumetric-no-noise",
                                 "--volumetric-integration", "basic"])
    first = run("strong_first", ["--volumetric-strong-fog", "5", "--volumetric-no-noise",
                                 "--volumetric-integration", "transmittance-first"])
    assert float(first[1]["radianceSum"]) < float(improved[1]["radianceSum"]) < float(basic[1]["radianceSum"]), \
        "analytical integration should fall between transmittance-first and basic in strong fog"
    run("minimum_steps", ["--volumetric-steps", "16"])
    run("zero_density", ["--volumetric-steps", "16", "--volumetric-density", "0"])
    dark = run("zero_light", ["--volumetric-light", "0"])
    assert float(dark[1]["radianceSum"]) == 0, "zero point light should be black"
    resized = run("resize_recompile", ["--smoke-resize-after", "2", "960", "640",
                                      "--smoke-recompile-after-setup"], frames=6)
    assert (resized[1]["width"], resized[1]["height"]) != (resized[6]["width"], resized[6]["height"])
    print("KUENGINE_VOLUMETRIC_CHECKS_OK deterministic_slots=1,2 animation density integration finite_extremes resize_recompile")


if __name__ == "__main__":
    main()
