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

- Pull Request merge가 기본. main 일반 직접 push 금지
- 긴급 수정 시 hotfix branch 작성 후 Pull Request 수행
- 상태 변경 시 빌드 및 테스트 통과 필요
- main force push는 의도적 복구 작업으로 허용. force push 후 남은 stale lock은 manual unlock으로 정리
- main 동기화와 작업 브랜치 최신화는 rebase 우선: `git pull --rebase origin main`

설정이나 계정이 꼬였으면 GitHub CLI 인증 후 `.\tools\set-git-config.ps1`를 실행한다.
Pull 후 Git identity 또는 Unreal Editor `LfsUserName` 경고가 뜨면 같은 스크립트로 repo-local Git identity, LFS 인증 helper, Editor user 설정을 맞춘다.

```powershell
winget install --id GitHub.cli -e
gh auth login -h github.com
.\tools\set-git-config.ps1
```

## Binary Asset lock 규칙

- `.uasset`, `.umap`, `.ubulk`, `.uexp` 수정 시 lock 필요
  - `task-setup.bat` 또는 `tools/set-git-config.ps1` 실행해야 read-only와 Editor checkout prompt 설정 적용
- Unreal Editor: `Check Out`만 수행. `Submit`, `Push`, `Unlock`, repository initialize 기능 사용 금지
  - Source Control 탭에서 `Git LFS 2 provider` 설정 필요
  - LFS user name은 GitHub login과 일치해야 한다. 경고가 뜨면 `tools/set-git-config.ps1` 실행
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
`set-git-config.ps1`은 GitHub login, commit email, LFS credential helper, Unreal Editor `LfsUserName`을 함께 맞춘다.

#### 3. 수정 후 Pull Request

```powershell
# 작업 사항 커밋
git add Client/Content/<path>/<asset>.uasset
git commit -m "feat: ..."

# push 및 PR 생성
.\task-push.bat
# 또는 수동 push
git push origin <branch>
```

작업 브랜치 push 후 Pull Request 생성.
main에 반영되면 변경된 애셋이 자동 unlock된다.

### 복구

```powershell
.\tools\manual-unlock.ps1 -Path Client/Content/<path>/<asset>.uasset
```

평상시에는 직접 unlock하지 않는다.
