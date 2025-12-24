# VanguardDT

### 폴더 구조
- `Firmware/` : STM32 NUCLEO-F103RB(CubeIDE) 펌웨어. 모터/서보 제어 + UART 패킷 수신
- `MQTT/` : 카메라/맵/모터 관련 브릿지(ROS 결과값 → MQTT/UART), HTTP 이미지 서빙 포함
- `QT/` : MQTT 구독 기반 GUI. 맵 렌더링/카메라 표시/로그/게이지 통합
- `ST Motor/` : 하드웨어 결선/핀맵/전원 구성 등 모터 시스템 물리 구성 문서화
- `MQTT_Interface_Spec.md` : MQTT 토픽/JSON payload 명세(시스템 인터페이스 기준 문서)
- `gaussian-splatting/` : 3D Gaussian Splatting 모델 학습 파이프라인. 이미지 전처리(Colmap)부터 학습, URL 배포를 위한 데이터 가공 코드 포함
- `models/` : 학습 완료 후 웹 뷰어에서 로드 가능한 형태(.splat)로 변환된 모델 파일 저장소
- `gif/` : 리드미 및 문서화를 위한 모델 구동 예시(애니메이션 GIF) 수록


## 1. 자율 주행
ROS 기반 SLAM/Planner가 만들어낸 **주행/지도 결과**를 받아, **STM32(NUCLEO-F103RB)** 가 모터/서보를 저수준 제어

- DC Motor(속도/방향) + Servo(조향) 저수준 제어 구현
- UART 패킷 기반으로 명령을 안정적으로 수신/처리
- enable / emergency stop 등 제어 플래그를 포함한 안전한 동작 흐름 구성

> 자세한 내용은 폴더 내 README.md 참조

## 2. SLAM + 맵 구성

## 3. MQTT와 Qt GUI
**MQTT** 로 GUI와 상태/명령/메타데이터를 주고받으며, **QT GUI** 가 맵/카메라/로그/게이지를 실시간 시각화하는 구조

### MQTT
- GUI ↔ 시스템 사이 통신을 MQTT 기반으로 설계/구현
- 카메라/맵 데이터는 **이미지 자체를 MQTT에 싣지 않고**, HTTP 서빙 + MQTT 메타(Url 등) 방식으로 경량화
- ROS에서 나온 결과값을 받아서
  - **STM32로 UART 패킷 전송**
  - 동시에 **GUI로 MQTT publish**
  형태로 동기화된 제어/모니터링 파이프라인 구축
> 자세한 내용은 폴더 내 README.md 참조


### Qt
- MQTT 구독 기반의 통합 대시보드 구현
<img src="./images/QtScreen.png" height="400">
  - Map Viewer(맵 + shot/pose 오버레이)
  - Camera Viewer(HTTP 이미지 로드)
  - Logs(상태/이벤트 기록)
  - Gauge(속도 지표 시각화)
- UI 구성/표시 로직과 통신 로직을 분리하고, 실시간 갱신을 고려한 구조로 구현
> 자세한 내용은 폴더 내 README.md 참조


## 4. 3D Gaussian Splatting Pipeline

| Meeting Room | Ogu |
| :---: | :---: |
| [![MeetingRoom](./gif/MeetingRoom.gif)](https://sikk806.github.io/VanguardDT/?url=https://huggingface.co/datasets/kyungbae/ssafy-3d-splat/resolve/main/models/test2.splat) | [![Ogu](./gif/ogu.gif)](https://sikk806.github.io/VanguardDT/?url=https://huggingface.co/datasets/kyungbae/ssafy-3d-splat/resolve/main/ogu.splat) |
| [🔗 Interactive Viewer](https://sikk806.github.io/VanguardDT/?url=https://huggingface.co/datasets/kyungbae/ssafy-3d-splat/resolve/main/models/test2.splat) | [🔗 Interactive Viewer](https://sikk806.github.io/VanguardDT/?url=https://huggingface.co/datasets/kyungbae/ssafy-3d-splat/resolve/main/ogu.splat) |

> 💡 **Tip:** 움직이는 이미지를 클릭하면 실시간 3D 뷰어로 연결됩니다.

</br>

프로젝트에서 실시간 3D 자산 생성을 위한 **가우시안 스플래팅(Gaussian Splatting) 학습 및 배포 파이프라인** 구축 </br>
저사양 환경(VRAM 4GB)에서의 최적화와 영상 입력부터 웹 배포까지의 **전과정 자동화**

### 1. 기술 스택 및 환경 설정
* **Core:** Python, PyTorch (CUDA 11.8), COLMAP, FFmpeg
* **Rendering:** 3D Gaussian Splatting (SIGGRAPH 2023)
* **Visualization:** Unreal Engine 5.4 (XVerse Plugin), WebGL (Splat Viewer)
* **Storage & Hosting:** Hugging Face (Model Storage), GitHub Pages (Web Hosting)
</br>

### 2. 전과정 자동화 파이프라인
데이터 전처리부터 모델 업로드, URL 배포까지 한 번의 명령어로 수행하는 **pipeline.py**를 구축

**실행 명령어 :**
```bash
python pipeline.py <video_data_path>
```

**자동화 단계**
1. **Preprocessing:** 이미지 리사이징 (640p -> 320p)
2. **SfM:** COLMAP을 통한 카메라 위치 추정
3. **Training:** 3D Gaussian Splatting 모델 학습 (7,000 iter)
4. **Convert:** .ply 파일을 웹 친화적인 .splat 확장자로 자동 변환
5. **Auto Upload:** Hugging Face API를 통해 학습 모델을 원격 저장소에 업로드 및 최종 결과 URL 반환
</br>

### 3. 결과물
* **Dataset:** [Hugging Face Repository](https://huggingface.co/datasets/kyungbae/ssafy-3d-splat/tree/main)
* **Live Demo:** [VanguardDT Online Viewer](https://sikk806.github.io/VanguardDT/?url=https://huggingface.co/datasets/kyungbae/ssafy-3d-splat/resolve/main/ogu.splat)
</br>
