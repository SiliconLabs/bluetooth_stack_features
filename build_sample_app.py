import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from shutil import copytree


def parse_xml(
    xml_file: Path,
    selected_examples: list[str],
    selected_boards: list[str],
) -> dict[Path, list[str]]:
    """Return each selected sample application and its target boards."""
    sample_apps: dict[Path, list[str]] = {}

    for descriptor in ET.parse(xml_file).getroot().findall("descriptors"):
        if selected_examples and descriptor.get("name") not in selected_examples:
            continue

        properties = {
            item.get("key"): item.get("value", "")
            for item in descriptor.findall("properties")
        }
        project_path = properties.get("projectFilePaths")
        if not project_path:
            continue

        compatible_boards = [
            board
            for board in properties.get("boardCompatibility", "").split()
            if any(character.isdigit() for character in board)
        ]
        sample_apps[Path(project_path)] = selected_boards or compatible_boards

    return sample_apps


def generate_project(slcp_path: Path, project_path: Path, board: str) -> None:
    """Generate one board-specific project with SLC."""
    print(f"Generating {project_path.name}...")
    subprocess.run(
        [
            "slc",
            "generate",
            "-p",
            str(slcp_path),
            "--new-project",
            "-d",
            str(project_path),
            f"-name={project_path.name}",
            "--output-type",
            "cmake",
            "--with",
            board,
        ],
        capture_output=True,
        text=True,
        check=True,
    )


def compile_project(cmake_path: Path) -> None:
    """Configure and build one generated project."""
    print(f"Building {cmake_path.parent.name}...")
    subprocess.run(
        ["cmake", "--workflow", "--preset", "project"],
        cwd=cmake_path,
        capture_output=True,
        text=True,
        check=True,
    )


def require_directory(path: Path, description: str) -> Path:
    """Return an expected directory or raise a descriptive error."""
    if not path.is_dir():
        raise FileNotFoundError(f"{description} not found: {path}")
    return path


def write_process_error(
    error: subprocess.CalledProcessError,
    log_path: Path,
) -> None:
    """Write both captured process streams to a diagnostic log."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        f"STDOUT:\n{error.stdout or ''}\n\nSTDERR:\n{error.stderr or ''}",
        encoding="utf-8",
    )


def write_error(error: OSError, log_path: Path) -> None:
    """Write a non-process filesystem or layout error."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(str(error), encoding="utf-8")


def build_apps(
    sample_apps: dict[Path, list[str]],
    sample_apps_root: Path,
    output_dir: Path,
) -> None:
    """Generate and build all selected projects, logging failures and continuing."""
    generated_root = Path("tmp") / "successful_generations"

    for app_path, boards in sample_apps.items():
        slcp_path = sample_apps_root / app_path
        app_name = app_path.stem

        for board in boards:
            project_name = f"{app_name}_{board}"
            project_path = generated_root / app_name / project_name

            try:
                generate_project(slcp_path, project_path, board)
            except subprocess.CalledProcessError as error:
                log_path = (
                    output_dir
                    / "failed_generations"
                    / app_name
                    / project_name
                    / "generation_error.log"
                )
                write_process_error(error, log_path)
                print(f"Failed to generate {project_name}; details: {log_path}")
                continue

            try:
                cmake_path = require_directory(
                    project_path / "cmake_gcc",
                    "Generated CMake directory",
                )
                compile_project(cmake_path)
                build_output = require_directory(
                    cmake_path / "build" / "base",
                    "Build output directory",
                )
                destination = output_dir / "successful_builds" / app_name / project_name
                copytree(build_output, destination, dirs_exist_ok=True)
            except subprocess.CalledProcessError as error:
                log_path = (
                    output_dir
                    / "failed_builds"
                    / app_name
                    / project_name
                    / "build_error.log"
                )
                write_process_error(error, log_path)
                print(f"Failed to build {project_name}; details: {log_path}")
            except OSError as error:
                log_path = (
                    output_dir
                    / "failed_builds"
                    / app_name
                    / project_name
                    / "artifact_error.log"
                )
                write_error(error, log_path)
                print(f"Failed to collect artifacts for {project_name}; details: {log_path}")


def parse_selection(value: str) -> list[str]:
    """Parse a comma-separated workflow input."""
    return [item.strip() for item in value.split(",") if item.strip()]


def main() -> None:
    sample_apps_root = Path(sys.argv[1])
    selected_examples = parse_selection(sys.argv[2])
    selected_boards = parse_selection(sys.argv[3])
    output_dir = Path(sys.argv[4])

    print("Parsing the template file...")
    sample_apps = parse_xml(
        sample_apps_root / "templates.xml",
        selected_examples,
        selected_boards,
    )
    print(f"Selected {len(sample_apps)} sample application(s).")
    print("Building apps...")
    build_apps(sample_apps, sample_apps_root, output_dir)


if __name__ == "__main__":
    main()
