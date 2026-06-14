# 작업 공통 규칙

- 생성물, cache, build output 업로드 금지
- 실행 경로 임의 추측 금지
- 문서, 구현, 테스트 일관성 유지

## 보안 규칙

API key, secret, credential, token 등 민감한 정보는 절대 커밋하지 않는다.

```text
.env
.env.local
secrets/
*.pem
*.key
OAuth token
service account private key
LLM API key
```

외부 접속이 가능한 API는 기본적으로 다음을 요구한다.

- 명시적 enable 옵션
- 인증 또는 access token
- CORS 제한
- User Directory path validation
- 내부 경로와 stack trace 비노출

## 구현 규칙

- 기존 기능과 호환되지 않는 변경은 `feat` 또는 `fix`로 명확히 구분하여 커밋한다.
- API 변경 시 `contracts` 함께 갱신
- 타인 코드 수정 최소화
- 객체지향 설계 원칙 준수하여 모듈 간 의존성 최소화

## `main` branch 규칙

- Pull Request merge만 허용. 직접 push 금지
- 긴급 수정 시 hotfix branch 작성 후 Pull Request 수행
- 상태 변경 시 빌드 및 테스트 통과 필요

## Binary Asset lock 규칙

- `.uasset`, `.umap`, `.ubulk`, `.uexp` 수정 시 lock 필요
  - `task-setup.bat` 또는 `tools/set-git-config.ps1` 실행해야 설정 적용
- Unreal Editor: `Check Out`만 수행. `Submit`, `Push`, `Unlock`, repository initialize 기능 사용 금지
- Checkout 실패 시 다른 사람이 lock한 상태. 해당 asset 수정 금지
- main 반영 후 변경된 애셋 자동 unlock됨
- dangling lock은 `tools/manual-unlock.ps1`로 해제 가능. 일반 작업 시 금지

### 수정 흐름

#### 1. 작업 브랜치에 최신 `main` 반영

```powershell
git fetch origin
git rebase origin/main
```

#### 2. Lock

Unreal Editor에서 `Git LFS 2 provider` 설정 후 Checkout 사용.

수동 설정:

```powershell
git lfs lock Client/Content/<path>/<asset>.uasset  # CLI checkout
git lfs locks --verify
```

Lock 성공 시 해당 asset이 writable 상태가 된다. 실패 시 다른 사람이 lock한 상태.

#### 3. 수정 후 Pull Request

```powershell
git add Client/Content/<path>/<asset>.uasset
git commit -m "feat: ..."
git push origin <branch>
```

작업 브랜치 push 후 Pull Request 생성.
main에 반영되면 변경된 애셋이 자동 unlock된다.

### 복구

```powershell
.\tools\manual-unlock.ps1 -Path Client/Content/<path>/<asset>.uasset
```

평상시에는 직접 unlock하지 않는다.
