# 아이콘 생성

프로젝트에 사용하는 아이콘은 일관된 디자인으로 제작하도록 한다.

## 파일 위치

- SVG 원본: `static/assets/icons/svg/<kebab-cased-icon-name>.svg`
- PNG 파생본: `static/assets/icons/png/<size>/<kebab-cased-icon-name>.png`
  - `84px-crop`: `96px` 결과물을 84x84로 crop한 버전. 이 파일을 Unreal Texture Asset으로 import한다.
- Unreal Texture Asset: `Client/Content/Textures/Icon/T_Icon_<PascalCasedIconName>.uasset`

## 원칙

- `static/assets/icons/svg`에 이미 비슷한 아이콘이 있는지 먼저 확인한다.
- 기존 SVG 아이콘을 참고하여 24x24 `viewBox`, line stroke, 크기감을 통일감있게 제작한다.
  - `fill="none"`, `stroke="currentColor"`, `stroke-width="2"`, `stroke-linecap="round"`, `stroke-linejoin="round"`, `style="color:#ffffff"`
- 레퍼런스 이미지가 있을 경우 픽셀 추적보다 기존 아이콘 세트에 맞게 재해석한다.
- XML 파싱, 렌더 확인으로 SVG가 깨지지 않는지 검증한다.

## AI 사용 지침
- 사용자가 렌더링을 요청하기 전까진 PNG 파생본과 Unreal Texture Asset을 만들지 않고 SVG를 먼저 완성하도록 안내한다.
- 기존 SVG 수정 시 렌더링과 Unreal Texture Asset 갱신을 잊지 않도록 매번 사용자에게 안내한다.

## PNG 렌더링

SVG 원본을 PNG로 렌더링할 때 다음 스크립트를 사용한다.

```powershell
.\static\assets\icons\render-png.ps1 <icon-name> -SkipInstall
```

ImageMagick 설치가 필요하고 자동 설치가 허용된 상황이면 `-SkipInstall`을 생략할 수 있다.

## Unreal Texture Asset

PNG 렌더링 후 `static/assets/icons/png/84px-crop/<icon-name>.png`를 Unreal Editor에서 `Client/Content/Textures/Icon`에 import한다.

- Asset 이름: `T_Icon_<PascalName>`
- Texture Group: `UI`
- Compression Settings: `Uncompressed (RGBA8)`
- `.uasset` 수정 시 Git LFS lock 또는 Unreal Editor `Check Out` 상태를 확인해야 한다.
