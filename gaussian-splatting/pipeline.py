import os
import sys
import time
import base64
import subprocess
from pathlib import Path
import shutil
import requests
import time
from huggingface_hub import HfApi

# ===== 설정 =====

# gaussian-splatting 루트(이 파일이 있는 위치 기준으로 현재 디렉터리 사용)
PROJECT_ROOT = Path(__file__).resolve().parent
OUTPUT_ROOT = PROJECT_ROOT / "output"

# 스크립트 이름(현 프로젝트 구조에 맞게)
CONVERT_SCRIPT = "convert.py"
TRAIN_SCRIPT = "train.py"
SPLAT_CONVERT_SCRIPT = "convert_splat.py"  # python convert_splat.py --input <ply> --output <splat> 형태라고 가정


def run_cmd(cmd, cwd=None):
    """서브프로세스 실행 헬퍼."""
    print("[CMD]", " ".join(cmd))
    result = subprocess.run(cmd, cwd=cwd, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"명령 실패 (코드 {result.returncode}): {' '.join(cmd)}")


def run_convert(source_path: Path):
    """convert.py 실행: COLMAP + resize."""
    print(f"[STEP] convert.py 실행 (source_path={source_path})")
    cmd = [
        sys.executable,
        CONVERT_SCRIPT,
        "-s", str(source_path),
        "--resize"
    ]
    run_cmd(cmd, cwd=PROJECT_ROOT)
    print("[STEP] convert.py 완료")

def apply_quarter_resolution(source_path: Path):
    """
    convert.py --resize 후 생성된 images_4를 기본 images로 교체하여
    train.py가 자동으로 1/4 해상도를 쓰도록 만든다.
    """
    images_dir = source_path / "images"
    images_4_dir = source_path / "images_4"

    if not images_4_dir.exists():
        raise RuntimeError("images_4 폴더가 없습니다. convert.py --resize 실행을 먼저 확인하세요.")

    # 기존 images 폴더 삭제
    if images_dir.exists():
        shutil.rmtree(images_dir)

    # images_4 를 images 로 rename
    shutil.move(str(images_4_dir), str(images_dir))
    print("[INFO] 1/4 해상도(images_4)를 기본 입력(images)으로 적용했습니다.")


def run_train(source_path: Path):
    """train.py 실행."""
    print(f"[STEP] train.py 실행 (source_path={source_path})")
    cmd = [
        sys.executable,
        TRAIN_SCRIPT,
        "-s", str(source_path),
        "--iterations", "7000",
        "--save_iterations", "7000",
        "--test_iterations", "7000",
    ]
    run_cmd(cmd, cwd=PROJECT_ROOT)
    print("[STEP] train.py 완료")


def find_latest_ply() -> Path:
    """
    output/ 아래에서 가장 최근에 수정된 폴더를 찾고,
    그 안의 point_cloud/iteration_7000/*.ply 파일 하나를 리턴.
    """
    if not OUTPUT_ROOT.exists():
        raise RuntimeError(f"output 폴더({OUTPUT_ROOT})가 없습니다.")

    dirs = [d for d in OUTPUT_ROOT.iterdir() if d.is_dir()]
    if not dirs:
        raise RuntimeError("output 폴더 안에 하위 폴더가 없습니다.")

    # 수정 시간 기준 최신 폴더 선택
    latest = max(dirs, key=lambda d: d.stat().st_mtime)
    print(f"[INFO] 최신 output 폴더: {latest}")

    pc_dir = latest / "point_cloud" / "iteration_7000"
    if not pc_dir.exists():
        raise RuntimeError(f"{pc_dir} 디렉터리가 없습니다. train.py 설정 또는 save_iterations를 확인하세요.")

    ply_files = list(pc_dir.glob("*.ply"))
    if not ply_files:
        raise RuntimeError(f"{pc_dir} 안에 .ply 파일이 없습니다.")
    if len(ply_files) > 1:
        print("[WARN] .ply 파일이 여러 개입니다. 첫 번째 파일만 사용합니다.")

    ply_path = ply_files[0]
    print(f"[INFO] 선택된 PLY 파일: {ply_path}")
    return ply_path


def convert_to_splat(ply_path: Path, splat_path: Path):
    """convert_splat.py 로 .ply -> .splat 변환 + 무결성 검사."""
    print(f"[STEP] convert_splat.py 실행 ({ply_path} -> {splat_path})")

    cmd = [
        sys.executable,
        SPLAT_CONVERT_SCRIPT,
        str(ply_path),               # positional argument
        "--output", str(splat_path)  # output option
    ]
    run_cmd(cmd, cwd=PROJECT_ROOT)

    # 파일 시스템 sync 기다리기
    time.sleep(0.2)

    # 파일 크기 검사
    if not splat_path.exists():
        raise RuntimeError("splat 파일이 생성되지 않았습니다!")

    file_size = splat_path.stat().st_size
    print(f"[CHECK] 생성된 splat 크기: {file_size} bytes")

    if file_size < 1024:
        raise RuntimeError("splat 파일이 비정상적으로 작습니다. 변환 실패 가능성!")

    # 4바이트 정렬 검사 (Float32Array 정렬 요구 조건)
    if file_size % 4 != 0:
        raise RuntimeError(
            f"splat 파일 byteLength({file_size})가 4의 배수가 아닙니다. "
            "Float32Array 생성 오류의 원인이 됩니다."
        )

    print("[STEP] .splat 변환 완료 (무결성 검사 통과)")


# 터미널 색상 코드 정의
class Colors:
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m' # 색상 초기화

api = HfApi()

def upload_huggingface(model_name: str, splat_path: Path):
    # 1. 설정 정보
    repo_id = "kyungbae/ssafy-3d-splat"
    base_viewer_url = "https://sikk806.github.io/VanguardDT/"
    
    # 2. 리포지토리 내 저장 경로 설정 (확장자 .splat 보장)
    if not model_name.endswith(".splat"):
        filename = f"{model_name}.splat"
    else:
        filename = model_name
        
    path_in_repo = f"models/{filename}"
    
    # 3. 파일 업로드 실행
    api.upload_file(
        path_or_fileobj=str(splat_path),
        path_in_repo=path_in_repo,
        repo_id=repo_id,
        repo_type="dataset",
    )
    
    # 4. 다운로드용 Direct URL 생성
    download_url = f"https://huggingface.co/datasets/{repo_id}/resolve/main/{path_in_repo}"
    
    # 5. 최종 결과 URL 생성 (뷰어 링크 + ?url= + 다운로드 링크)
    final_url = f"{base_viewer_url}?url={download_url}"
    
    # 최종 결과 출력 (무지개/파란색 스타일)
    print("\n" + "="*60)
    print(f"{Colors.GREEN}✨ 업로드가 성공적으로 완료되었습니다!{Colors.END}")
    print(f"{Colors.BOLD}🔗 최종 결과 확인 (Ctrl+Click):{Colors.END}")
    # 클릭할 링크를 파란색 + 밑줄로 강조
    print(f"{Colors.BLUE}{Colors.UNDERLINE}{final_url}{Colors.END}")
    print("="*60 + "\n")


def main():
    source_path = (PROJECT_ROOT / sys.argv[1]).resolve()

    # 모델 이름 자동 추출
    model_name = source_path.name
    total_start = time.time()
    """
    사용법:
      python pipeline.py data/ogu

    전제:
      - 라즈베리파이가 찍은 원본 이미지는 data/ogu/input 안에 존재
      - convert.py, train.py, convert_splat.py 는 PROJECT_ROOT 에 있음
    """
    if len(sys.argv) < 2:
        print("사용법: python pipeline.py <source_path>")
        print("예:    python pipeline.py data/ogu")
        sys.exit(1)

    source_path = (PROJECT_ROOT / sys.argv[1]).resolve()
    if not source_path.exists():
        print(f"에러: {source_path} 경로가 존재하지 않습니다.")
        sys.exit(1)

    print(f"[PIPELINE] 시작 (source_path={source_path})")

    # 1) convert
    run_convert(source_path)

    # 2) train
    apply_quarter_resolution(source_path)
    run_train(source_path)

    # 3) 최신 ply 찾기
    ply_path = find_latest_ply()

    # 4) splat 변환 (output/<uuid>/model.splat 으로 저장)
    splat_path = ply_path.parent / "model.splat"
    convert_to_splat(ply_path, splat_path)

    upload_huggingface(model_name, splat_path)

    print("[PIPELINE] 전체 작업 완료.")
    total_end = time.time()
    print(f"\n[TIME] 전체 파이프라인 소요 시간: {total_end - total_start:.2f}초")


if __name__ == "__main__":
    main()
