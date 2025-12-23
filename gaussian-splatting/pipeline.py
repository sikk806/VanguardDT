import os
import sys
import time
import subprocess
from pathlib import Path
import shutil
from huggingface_hub import HfApi, logging
from huggingface_hub.utils import disable_progress_bars
from tqdm import tqdm  # 진행바 라이브러리 추가

# ===== 설정 =====
PROJECT_ROOT = Path(__file__).resolve().parent
OUTPUT_ROOT = PROJECT_ROOT / "output"

CONVERT_SCRIPT = "convert.py"
TRAIN_SCRIPT = "train.py"
SPLAT_CONVERT_SCRIPT = "convert_splat.py"

logging.set_verbosity_error()

class Colors:
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'

api = HfApi()

def run_cmd_silent(cmd, cwd=None):
    """로그를 출력하지 않고 명령어를 실행합니다."""
    # stdout, stderr를 DEVNULL로 보내서 화면에 로그가 찍히지 않게 함
    result = subprocess.run(
        cmd, 
        cwd=cwd, 
        stdout=subprocess.DEVNULL, 
        stderr=subprocess.DEVNULL, 
        text=True
    )
    if result.returncode != 0:
        raise RuntimeError(f"명령 실패 (코드 {result.returncode})")

def run_convert(source_path: Path):
    cmd = [sys.executable, CONVERT_SCRIPT, "-s", str(source_path), "--resize"]
    run_cmd_silent(cmd, cwd=PROJECT_ROOT)

def apply_quarter_resolution(source_path: Path):
    images_dir = source_path / "images"
    images_4_dir = source_path / "images_4"
    if not images_4_dir.exists():
        raise RuntimeError("images_4 폴더가 없습니다.")
    if images_dir.exists():
        shutil.rmtree(images_dir)
    shutil.move(str(images_4_dir), str(images_dir))

def run_train(source_path: Path):
    cmd = [
        sys.executable, TRAIN_SCRIPT,
        "-s", str(source_path),
        "--iterations", "7000",
        "--save_iterations", "7000",
        "--test_iterations", "7000",
    ]
    run_cmd_silent(cmd, cwd=PROJECT_ROOT)

def find_latest_ply() -> Path:
    dirs = [d for d in OUTPUT_ROOT.iterdir() if d.is_dir()]
    if not dirs: raise RuntimeError("output 폴더가 비어있습니다.")
    latest = max(dirs, key=lambda d: d.stat().st_mtime)
    pc_dir = latest / "point_cloud" / "iteration_7000"
    ply_files = list(pc_dir.glob("*.ply"))
    return ply_files[0]

def convert_to_splat(ply_path: Path, splat_path: Path):
    cmd = [sys.executable, SPLAT_CONVERT_SCRIPT, str(ply_path), "--output", str(splat_path)]
    run_cmd_silent(cmd, cwd=PROJECT_ROOT)
    
    # 무결성 검사 (기존 로직 유지)
    if not splat_path.exists() or splat_path.stat().st_size % 4 != 0:
        raise RuntimeError("Splat 파일 무결성 검사 실패")

def upload_huggingface(model_name: str, splat_path: Path):
    disable_progress_bars()
    repo_id = "kyungbae/ssafy-3d-splat"
    base_viewer_url = "https://sikk806.github.io/VanguardDT/"
    filename = model_name if model_name.endswith(".splat") else f"{model_name}.splat"
    path_in_repo = f"models/{filename}"
    
    api.upload_file(
        path_or_fileobj=str(splat_path),
        path_in_repo=path_in_repo,
        repo_id=repo_id,
        repo_type="dataset",
    )
    
    download_url = f"https://huggingface.co/datasets/{repo_id}/resolve/main/{path_in_repo}"
    return f"{base_viewer_url}?url={download_url}"

def main():
    if len(sys.argv) < 2:
        print("사용법: python pipeline.py <source_path>")
        sys.exit(1)

    source_path = (PROJECT_ROOT / sys.argv[1]).resolve()
    model_name = source_path.name
    total_start = time.time()

    # --- 진행 바 설정 (총 5단계) ---
    steps = [
        "COLMAP 데이터 변환 (Convert)",
        "해상도 최적화 (Resize)",
        "가우시안 트레이닝 (Train)",
        "Splat 포맷 변환 (Export)",
        "허깅페이스 업로드 (Upload)"
    ]
    
    print(f"\n{Colors.BOLD}[3D Splat 자동화 파이프라인 시작]{Colors.END}\n")

    final_url = ""
    
    with tqdm(total=len(steps), desc="전체 공정 진행률", bar_format="{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}]") as pbar:
        try:
            # 1단계: Convert
            pbar.set_description(f"{Colors.CYAN}{steps[0]}{Colors.END}")
            run_convert(source_path)
            pbar.update(1)

            # 2단계: Resize 적용
            pbar.set_description(f"{Colors.CYAN}{steps[1]}{Colors.END}")
            apply_quarter_resolution(source_path)
            pbar.update(1)

            # 3단계: Train
            pbar.set_description(f"{Colors.CYAN}{steps[2]}{Colors.END}")
            run_train(source_path)
            pbar.update(1)

            # 4단계: Splat 변환
            pbar.set_description(f"{Colors.CYAN}{steps[3]}{Colors.END}")
            ply_path = find_latest_ply()
            splat_path = ply_path.parent / "model.splat"
            convert_to_splat(ply_path, splat_path)
            pbar.update(1)

            # 5단계: Upload
            pbar.set_description(f"{Colors.CYAN}{steps[4]}{Colors.END}")
            final_url = upload_huggingface(model_name, splat_path)
            pbar.update(1)

            pbar.close()

        except Exception as e:
            print(f"\n\n{Colors.BOLD}[ERROR]{Colors.END} 작업 중 오류가 발생했습니다: {e}")
            sys.exit(1)

    total_end = time.time()
    print("\n" + "="*60)
    print(f"{Colors.GREEN}✨ 모든 작업이 완료되었습니다! (소요시간: {total_end - total_start:.2f}초){Colors.END}")
    print(f"{Colors.BOLD}🔗 결과 확인 링크:{Colors.END}")
    print(f"{Colors.BLUE}{Colors.UNDERLINE}{final_url}{Colors.END}")
    print("="*60 + "\n")


if __name__ == "__main__":
    main()