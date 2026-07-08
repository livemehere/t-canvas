# TCanvas

TCanvas는 스크린샷과 시각 메모에 빠르게 주석을 추가할 수 있는 가벼운 macOS 캔버스 도구입니다.
C++, GLFW, OpenGL, ImGui, Skia로 만들어졌습니다.

TCanvas는 빠른 마크업 워크플로에 집중합니다. 도형을 그리고, 영역을 강조하고, 민감한 정보를 블러 처리하고,
결과를 복사하거나 여러 크기의 에셋으로 내보낼 수 있습니다.

<img width="1555" height="909" alt="image" src="https://github.com/user-attachments/assets/4961d174-f1ae-4b58-8549-757857cfde38" />

## 기능

* 사각형, 원, 선, 화살표, 텍스트, 이미지, 블러 브러시와 컬러 브러시를 그릴 수 있습니다.
* 사각형, 원, 선, 화살표는 드래그해서 바로 생성할 수 있습니다.
* 선택한 도형은 리사이즈 핸들을 사용해 크기를 조정하고 변형할 수 있습니다.
* 크기 조정 중 Shift를 누르면 비율을 유지할 수 있습니다.
* 박스형 도형의 크기 조정 중 Option을 누르면 중심을 기준으로 크기가 변합니다.
* 선이나 화살표를 그릴 때 Shift를 누르면 수평 또는 수직 방향으로 제한할 수 있습니다.
* Space를 누른 상태로 드래그하면 캔버스를 이동할 수 있습니다.
* 마우스 휠로 확대/축소할 수 있으며, Shift + Wheel을 사용하면 더 세밀하게 조절할 수 있습니다.
* 브러시를 선택한 상태에서 Option + Wheel을 사용하면 브러시 크기를 조절할 수 있습니다.
* 그리드와 주변 도형의 모서리/중심에 스냅할 수 있습니다.
* 드래그 선택 또는 Cmd + A로 여러 도형을 선택할 수 있습니다.
* Cmd + C / Cmd + V로 도형을 복사/붙여넣기 할 수 있습니다.
* 선택한 내용을 이미지로 복사하여 다른 앱에 붙여넣을 수 있습니다.
* 시스템 클립보드의 스크린샷이나 이미지를 붙여넣을 수 있습니다.
* 선택한 내용을 PNG로 내보낼 수 있습니다.
* 하나의 선택 영역에서 여러 크기 변형을 내보낼 수 있습니다.
* 레이어 트리에서 도형을 숨기고, 잠그고, 순서를 변경하고, 선택할 수 있습니다.
* 도형 타입별 기본 스타일을 설정할 수 있습니다.

<img width="1556" height="906" alt="image" src="https://github.com/user-attachments/assets/d97ea751-e3fe-44aa-8b13-2898a4573ea4" />

## 캔버스 도구

하단 툴바에는 주요 도구들이 있습니다.

도구 단축키 설명
Select S 도형을 선택, 이동, 크기 조정, 변형합니다.
Pan P 캔버스를 이동합니다. Space를 누른 상태로도 사용할 수 있습니다.
Rect R 드래그해서 사각형을 생성합니다.
Circle C 드래그해서 원 또는 타원을 생성합니다.
Line L 드래그해서 선을 생성합니다. Shift를 누르면 수평/수직 선으로 제한됩니다.
Arrow A 드래그해서 화살표를 생성합니다. Shift를 누르면 수평/수직 화살표로 제한됩니다.
Text T 텍스트 박스를 만들고 내용을 편집합니다.
Image I 이미지 파일을 도형으로 가져옵니다.
Brush B 블러 영역 또는 컬러 브러시 스트로크를 칠합니다.

<img width="941" height="177" alt="image" src="https://github.com/user-attachments/assets/3727ff81-f79e-4df0-a830-465aebc5fb54" />

## 레이어

왼쪽 레이어 패널은 모든 도형을 z-order 순서로 보여줍니다.
위에 있는 항목일수록 아래 항목보다 앞에 렌더링됩니다.

### 레이어 동작:

* 레이어를 클릭하면 해당 도형을 선택합니다.
* Shift + Click으로 두 번째 레이어를 클릭하면 범위 선택을 할 수 있습니다.
* 레이어를 드래그해서 z-index 순서를 변경할 수 있습니다.
* V 키 또는 표시 버튼으로 선택한 레이어의 표시 여부를 전환할 수 있습니다.
* K 키 또는 잠금 버튼으로 선택한 레이어 잠금 여부를 전환할 수 있습니다.

잠긴 레이어는 캔버스에서 선택할 수 없습니다. 숨겨진 레이어는 렌더링, 선택, 블러, 내보내기 대상에서 제외됩니다.

<img width="622" height="687" alt="image" src="https://github.com/user-attachments/assets/310696fe-68f1-42f3-91ee-d7539a31061c" />

## 인스펙터

오른쪽 인스펙터에서는 선택한 도형의 속성을 편집할 수 있습니다.

단일 도형을 선택한 경우 다음 속성을 편집할 수 있습니다.

* 이름
* 위치
* 크기
* 회전
* 채우기 색상
* 선 색상
* 선 두께
* 모서리 반경
* 배경 블러
* 블러 강도
* 텍스트 도형의 텍스트 내용
* 브러시 도형의 브러시 크기

여러 도형을 선택한 경우 공통 스타일 속성을 함께 편집할 수 있습니다.

<img width="2230" height="1402" alt="image" src="https://github.com/user-attachments/assets/36a49940-3c3c-453e-8f92-0e8e827578bb" />

## 블러 워크플로

TCanvas는 두 가지 방식의 블러를 지원합니다.

* Brush 도구를 사용해 블러 마스크, 컬러 스트로크, 또는 둘 다 칠할 수 있습니다.
* 도형에서 Background Blur를 활성화해 해당 도형 뒤의 내용을 블러 처리할 수 있습니다.

블러 영역은 선택한 내용을 복사하거나 내보낼 때 함께 포함됩니다.

<img width="703" height="457" alt="image" src="https://github.com/user-attachments/assets/25b0c177-7243-4009-b13b-7799f7e2c179" />

## 클립보드

클립보드 동작은 스크린샷 주석 워크플로에 맞춰 설계되어 있습니다.

* Cmd + C는 선택한 도형을 TCanvas 내부에 복사합니다.
* 같은 Cmd + C 동작은 렌더링된 PNG 이미지도 시스템 클립보드에 함께 기록합니다.
* Cmd + X는 선택한 도형을 잘라내며, 같은 내부 복사 데이터와 PNG 클립보드 데이터를 기록합니다.
* Slack, Notes, Figma 또는 다른 앱에 붙여넣어 렌더링된 이미지를 사용할 수 있습니다.
* 시스템 클립보드에 스크린샷이나 이미지가 있는 경우 Cmd + V로 이미지 도형으로 가져옵니다.
* 클립보드가 아직 TCanvas의 복사 데이터인 경우 Cmd + V로 복제된 도형을 붙여넣습니다.

오른쪽 하단 선택 HUD는 선택 영역의 내보내기/복사 크기를 width x height 형식으로 보여줍니다.

<img width="2058" height="1440" alt="image" src="https://github.com/user-attachments/assets/7ab5b822-209f-4bcf-a34f-dcd7dfc96d8a" />

## 내보내기

툴바에서 Export를 열어 현재 선택 영역을 PNG로 내보낼 수 있습니다.

내보내기는 다음 기능을 지원합니다.

* 사용자 지정 너비와 높이
* 실시간 PNG 미리보기
* 클립보드로 복사
* 파일로 저장
* 여러 크기 변형

여러 변형을 추가하면 TCanvas는 다음과 같이 크기 접미사를 붙여 파일을 저장합니다.

```bash
tc_export_800x600.png
tc_export_1600x1200.png
```

<img width="3104" height="1634" alt="image" src="https://github.com/user-attachments/assets/ffb9e515-5438-4d7c-901a-0f0087920da9" />

## 환경설정

툴바에서 Settings를 열어 도형 타입별 기본 스타일 프리셋을 설정할 수 있습니다.

환경설정 항목은 다음과 같습니다.

* 채우기 색상
* 선 색상
* 선 두께
* 모서리 반경
* 배경 블러
* 블러 강도
* 브러시 크기

설정은 로컬의 다음 경로에 저장됩니다.

```bash
~/Library/Application Support/TCanvas/preferences.ini
```

<img width="960" height="759" alt="image" src="https://github.com/user-attachments/assets/eba80097-f9f1-4b57-afd4-1752f86f6f42" />
